/*
 * Copyright 2025 Budapest University of Technology and Economics
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Accesses of an object through a pointer cast to another type (issue #11,
// README 7.5 and 8.7). C forbids them (C11 6.5p7). In LLVM IR, memory has no
// types, so the CIR uses them freely.
//
// The check: after the translation, reportReinterpretations counts the accesses
// that remain in the output and writes a warning.
//
// The union slots: clang applies the calling convention of the target already
// in the CIR. A small struct that a function returns or receives by value
// travels as one or two integer, pointer or floating-point values. The CIR
// converts between the struct and these values through a local variable: it
// stores one type and loads the other through a `cir.cast bitcast` of the
// variable's address. Such a variable becomes a union of all the types that
// the CIR reads it as, and each bitcast becomes the member of its type. C11
// 6.5.2.3 (note 95) defines a read through another member: it reinterprets the
// bytes.
//
// A struct goes into and out of the union member by member:
//   * The union is zeroed at its declaration. A copy of a whole struct would
//     also copy its padding, which nothing wrote, and the other member would
//     then read uninitialized bytes.
//   * After a copy of a whole struct, clang's TypeSanitizer gives the copy the
//     type of the source member, and reports each later read of the copy. C
//     permits these reads, but the reports would hide real ones.
//
// The union slots need only this file and one short call from each of the
// alloca, cast, load, store and copy handlers.

#include "Mapper.h"
#include "RecordLayout.h"

#include <clang/CIR/Dialect/IR/CIRDialect.h>
#include <clang/CIR/Dialect/IR/CIRTypes.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <cctype>
#include <functional>

namespace cir2c {

namespace {

bool isByteType(mlir::Type t) {
  auto it = mlir::dyn_cast<cir::IntType>(t);
  return it && it.getWidth() == 8;
}

// A vtable pointer, a pointer to void and a pointer to a function are all
// `void *` in the output.
bool isVoidPointerInC(mlir::Type t) {
  if (mlir::isa<cir::VPtrType>(t)) return true;
  auto pointer = mlir::dyn_cast<cir::PointerType>(t);
  return pointer && mlir::isa<cir::VoidType, cir::FuncType>(pointer.getPointee());
}

bool isDataMember(cir::RecordType record, size_t i) {
  llvm::ArrayRef<mlir::Type> members = record.getMembers();
  llvm::ArrayRef<cir::RecordMemberKind> kinds = record.getMemberKinds();
  if (kinds.size() != members.size()) return true;
  return (kinds[i] == cir::RecordMemberKind::Data ||
          kinds[i] == cir::RecordMemberKind::BitField) &&
         !memberOccupiesNoStorage(members[i], kinds[i]);
}

// True when an object of type \p outer has a subobject of type \p inner at
// offset 0: its first member, the first element, or a member of a union.
bool startsWith(mlir::Type outer, mlir::Type inner) {
  while (outer != inner) {
    if (auto array = mlir::dyn_cast<cir::ArrayType>(outer)) {
      outer = array.getElementType();
      continue;
    }
    auto record = mlir::dyn_cast<cir::RecordType>(outer);
    if (!record || !record.isComplete()) return false;
    llvm::ArrayRef<mlir::Type> members = record.getMembers();
    if (record.isUnion()) {
      for (mlir::Type member : members)
        if (startsWith(member, inner)) return true;
      return false;
    }
    size_t i = 0;
    while (i < members.size() && !isDataMember(record, i)) ++i;
    if (i == members.size()) return false;
    outer = members[i];
  }
  return true;
}

// True when every use of the pointer \p v is a load from it, a store to it, a
// copy, or a member access with the same uses. The handlers then write each
// use with the union member expression, and no pointer to the member escapes.
bool usedOnlyForAccess(mlir::Value v) {
  for (mlir::OpOperand &use : v.getUses()) {
    mlir::Operation *user = use.getOwner();
    if (auto load = mlir::dyn_cast<cir::LoadOp>(user)) {
      if (load.getMemOrderAttr() || load.getIsVolatile()) return false;
      continue;
    }
    if (auto store = mlir::dyn_cast<cir::StoreOp>(user)) {
      if (store.getMemOrderAttr() || store.getIsVolatile()) return false;
      if (use.getOperandNumber() != 1) return false;
      continue;
    }
    if (auto member = mlir::dyn_cast<cir::GetMemberOp>(user)) {
      if (!usedOnlyForAccess(member.getResult())) return false;
      continue;
    }
    if (mlir::isa<cir::CopyOp>(user)) continue;
    return false;
  }
  return true;
}

bool isAccess(mlir::OpOperand &use) {
  mlir::Operation *user = use.getOwner();
  if (mlir::isa<cir::LoadOp, cir::GetMemberOp, cir::CopyOp>(user)) return true;
  return mlir::isa<cir::StoreOp>(user) && use.getOperandNumber() == 1;
}

// An expression that names an object without side effects: an identifier with
// member accesses and constant or identifier subscripts.
bool isPlainLvalue(const std::string &e) {
  if (e.empty() || !(std::isalpha((unsigned char)e[0]) || e[0] == '_')) return false;
  for (size_t i = 0; i < e.size(); ++i) {
    char c = e[i];
    if (std::isalnum((unsigned char)c) || c == '_' || c == '.' || c == '[' || c == ']')
      continue;
    if (c == '-' && i + 1 < e.size() && e[i + 1] == '>') { ++i; continue; }
    return false;
  }
  return true;
}

// The data layout of the CIR module, which is a module nested in \p module.
std::unique_ptr<mlir::DataLayout> findDataLayout(mlir::ModuleOp module) {
  std::unique_ptr<mlir::DataLayout> layout;
  module->walk([&](mlir::ModuleOp m) {
    if (!layout && m->hasAttr("dlti.dl_spec")) layout = std::make_unique<mlir::DataLayout>(m);
  });
  return layout;
}

} // namespace

bool Mapper::reinterpretsObject(mlir::Type object, mlir::Type view) {
  if (object == view) return false;
  // Two CIR types with one C type, `void *`. (mapTypeToC is not usable here:
  // it numbers the anonymous records in the sequence of the first request.)
  if (isVoidPointerInC(object) && isVoidPointerInC(view)) return false;
  // C11 6.5p7 permits an access through a character type. A `void *` or a
  // `char *` source tells nothing about the type of the object it points to.
  for (mlir::Type t : {object, view})
    if (mlir::isa<cir::VoidType, cir::FuncType>(t) || isByteType(t)) return false;
  for (mlir::Type t : {object, view})
    if (auto record = mlir::dyn_cast<cir::RecordType>(t))
      if (!record.isComplete()) return false;
  // The signed or unsigned type that corresponds to the type of the object.
  auto objectInt = mlir::dyn_cast<cir::IntType>(object);
  auto viewInt = mlir::dyn_cast<cir::IntType>(view);
  if (objectInt && viewInt && objectInt.getWidth() == viewInt.getWidth()) return false;
  // A subobject at offset 0 (C11 6.7.2.1p15), or an aggregate that has the
  // object as its first member and is not larger (C11 6.5p7).
  if (startsWith(object, view)) return false;
  if (dataLayout_ && startsWith(view, object) &&
      dataLayout_->getTypeSize(view) <= dataLayout_->getTypeSize(object))
    return false;
  return true;
}

void Mapper::planUnionSlots(mlir::ModuleOp module) {
  if (!dataLayout_) dataLayout_ = findDataLayout(module);
  module->walk([&](cir::CastOp cast) {
    if (cast.getKind() != cir::CastKind::bitcast) return;
    auto alloca = cast.getSrc().getDefiningOp<cir::AllocaOp>();
    if (!alloca || alloca.getDynAllocSize()) return;
    mlir::Value slot = alloca.getResult();
    if (isAtomicAlloca(slot) || isVolatileAlloca(slot)) return;
    mlir::Type own = alloca.getAllocaType();
    auto viewPointer = mlir::dyn_cast<cir::PointerType>(cast.getType());
    if (!viewPointer) return;
    mlir::Type view = viewPointer.getPointee();
    // An array or a vector cannot be a union member in the form that the
    // handlers expect. The check of reportReinterpretations shows them.
    for (mlir::Type t : {own, view})
      if (mlir::isa<cir::ArrayType, cir::VectorType>(t)) return;
    if (!reinterpretsObject(own, view) || !usedOnlyForAccess(cast.getResult())) return;
    std::vector<mlir::Type> &types = unionSlotTypes_[slot];
    if (types.empty()) types.push_back(own);
    if (std::find(types.begin(), types.end(), view) == types.end()) types.push_back(view);
    unionSlotCasts_.insert(cast.getOperation());
  });
  // The C names of these values are a member of a union slot, or a member of
  // such a member.
  std::function<void(mlir::Value)> addView = [&](mlir::Value v) {
    unionSlotViews_.insert(v);
    for (mlir::Operation *user : v.getUsers())
      if (auto member = mlir::dyn_cast<cir::GetMemberOp>(user))
        if (member->getOperand(0) == v) addView(member.getResult());
  };
  for (auto &slot : unionSlotTypes_) addView(slot.first);
  for (mlir::Operation *cast : unionSlotCasts_) addView(cast->getResult(0));
}

bool Mapper::emitUnionSlot(mlir::Value result, const std::string &name, std::ostream &out) {
  auto it = unionSlotTypes_.find(result);
  if (it == unionSlotTypes_.end()) return false;
  out << "  union {";
  for (size_t i = 0; i < it->second.size(); ++i)
    out << " " << mapTypeToC(it->second[i]) << " v" << i << ";";
  out << " } " << name << ";\n";
  ensureMemsetDeclared();
  out << "  memset(&" << name << ", 0, sizeof " << name << ");\n";
  unionSlotNames_[result] = name;
  setName(result, name + ".v0");
  markAsDirectAccess(result);
  return true;
}

bool Mapper::mapUnionSlotView(mlir::Operation *castOp) {
  if (!unionSlotCasts_.count(castOp)) return false;
  auto cast = mlir::cast<cir::CastOp>(castOp);
  mlir::Value slot = cast.getSrc();
  auto name = unionSlotNames_.find(slot);
  if (name == unionSlotNames_.end()) return false;
  mlir::Type view = mlir::cast<cir::PointerType>(cast.getType()).getPointee();
  const std::vector<mlir::Type> &types = unionSlotTypes_[slot];
  size_t i = std::find(types.begin(), types.end(), view) - types.begin();
  setName(cast.getResult(), name->second + ".v" + std::to_string(i));
  markAsDirectAccess(cast.getResult());
  return true;
}

std::string Mapper::memberwiseCopy(mlir::Type type, const std::string &dst,
                                   const std::string &src) {
  if (auto array = mlir::dyn_cast<cir::ArrayType>(type)) {
    if (!mlir::isa<cir::RecordType, cir::ArrayType>(array.getElementType())) {
      ensureMemcpyDeclared();
      return "  memcpy(" + dst + ", " + src + ", sizeof(" + dst + "));\n";
    }
    std::string copy;
    for (uint64_t k = 0; k < array.getSize(); ++k) {
      std::string index = "[" + std::to_string(k) + "]";
      copy += memberwiseCopy(array.getElementType(), dst + index, src + index);
    }
    return copy;
  }
  auto record = mlir::dyn_cast<cir::RecordType>(type);
  if (!record || !record.isComplete() || record.isUnion())
    return "  " + dst + " = " + src + ";\n";
  std::string tag = (record.getName() && !record.getName().getValue().empty())
                        ? recordCName(record.getName())
                        : anonRecordCName(record);
  llvm::ArrayRef<mlir::Type> members = record.getMembers();
  std::string copy;
  for (size_t i = 0; i < members.size(); ++i) {
    if (!isDataMember(record, i)) continue;
    std::string field = lookupFieldName(tag, static_cast<int>(i));
    if (field.empty()) field = "__field" + std::to_string(i);
    copy += memberwiseCopy(members[i], dst + "." + field, src + "." + field);
  }
  return copy;
}

bool Mapper::copyThroughUnionSlot(mlir::Value dstAddr, mlir::Value srcAddr, mlir::Type type,
                                  const std::string &dstExpr, const std::string &srcExpr,
                                  bool declareDst, std::ostream &out) {
  bool touchesSlot = (dstAddr && unionSlotViews_.count(dstAddr)) ||
                     (srcAddr && unionSlotViews_.count(srcAddr));
  auto record = mlir::dyn_cast<cir::RecordType>(type);
  if (!touchesSlot || !record || !record.isComplete() || record.isUnion()) return false;
  // `*p.x` would be `*(p.x)`.
  std::string dst = isPlainLvalue(dstExpr) ? dstExpr : "(" + dstExpr + ")";
  std::string src = srcExpr;
  if (!isPlainLvalue(src)) {
    src = freshName("t");
    out << "  " << mapTypeToC(type) << " " << src << " = " << srcExpr << ";\n";
  }
  if (declareDst) out << "  " << mapTypeToC(type) << " " << dstExpr << ";\n";
  out << memberwiseCopy(type, dst, src);
  return true;
}

void Mapper::reportReinterpretations(mlir::ModuleOp module) {
  if (!dataLayout_) dataLayout_ = findDataLayout(module);
  unsigned total = 0, onLocals = 0;
  std::string first;
  module->walk([&](cir::CastOp cast) {
    if (cast.getKind() != cir::CastKind::bitcast || unionSlotCasts_.count(cast.getOperation())) return;
    auto from = mlir::dyn_cast<cir::PointerType>(cast.getSrc().getType());
    auto to = mlir::dyn_cast<cir::PointerType>(cast.getType());
    if (!from || !to || !reinterpretsObject(from.getPointee(), to.getPointee())) return;
    if (mapTypeToC(from.getPointee()) == mapTypeToC(to.getPointee())) return;
    bool accessed = false;
    for (mlir::OpOperand &use : cast.getResult().getUses()) accessed = accessed || isAccess(use);
    if (!accessed) return;
    ++total;
    if (cast.getSrc().getDefiningOp<cir::AllocaOp>()) ++onLocals;
    if (first.empty()) {
      auto func = cast->getParentOfType<cir::FuncOp>();
      first = (func ? func.getSymName().str() + ": " : std::string()) +
              mapTypeToC(from.getPointee()) + " as " + mapTypeToC(to.getPointee());
    }
  });
  if (total == 0) return;
  llvm::errs() << "cir2c: warning: the output accesses an object through a pointer "
                  "cast to another type at "
               << total << " places, " << onLocals
               << " of them on local variables (C11 6.5p7). The first: " << first << "\n";
}

} // namespace cir2c

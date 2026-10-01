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
// README 8.7). C forbids them (C11 6.5p7). In LLVM IR, memory has no types,
// so the CIR uses them freely.
//
// After the translation, reportReinterpretations counts the accesses that
// remain in the output and writes a warning.

#include "Mapper.h"
#include "RecordLayout.h"

#include <clang/CIR/Dialect/IR/CIRDialect.h>
#include <clang/CIR/Dialect/IR/CIRTypes.h>
#include <llvm/Support/raw_ostream.h>

#include <cctype>

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

bool isAccess(mlir::OpOperand &use) {
  mlir::Operation *user = use.getOwner();
  if (mlir::isa<cir::LoadOp, cir::GetMemberOp, cir::CopyOp>(user)) return true;
  return mlir::isa<cir::StoreOp>(user) && use.getOperandNumber() == 1;
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

void Mapper::reportReinterpretations(mlir::ModuleOp module) {
  if (!dataLayout_) dataLayout_ = findDataLayout(module);
  unsigned total = 0, onLocals = 0;
  std::string first;
  module->walk([&](cir::CastOp cast) {
    if (cast.getKind() != cir::CastKind::bitcast) return;
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

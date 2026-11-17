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

// Virtual bases and offset-to-top (issue #3 part 2, issue #18).
//
// An object finds a virtual base subobject, a virtual thunk its vcall offset,
// and dynamic_cast<void *> the complete object through integers in the vtable,
// before the address point. cir2c writes each vtable as a `void *` array, so
// such an integer is an integer constant cast to `void *` in the initializer
// (C11 6.6p9), and a read converts it back (C11 6.3.2.3p5 and p6: the value is
// implementation-defined, the inverse of the first conversion on the usual
// targets).
//
// The CIR reads an entry through casts and constant strides from the loaded
// vtable pointer, as bytes or as 64-bit integers. These values are views: they
// emit no C, because a C read of a `void *` element through `long *` would
// violate C11 6.5p7. The load at the end reads the `void *` element. Any other
// use of a view stops the translation.
//
// For a local or global variable of a class type, the object is complete, so
// its vtable pointer holds the primary address point of the vtable of that
// class. The entry is then a constant, and the vtable pointer is not read.
// This also holds when the constructor of the variable is unknown
// (--externalize-std), which sets the vtable pointer to an unknown value. A
// read through the vtable pointer of an object of a library class whose vtable
// is not in the module is not possible, in either mode: libc++ instantiates its
// streams in its compiled library, so no code of the output sets their vtable
// pointers, and cir2c stops with an error (issue #32).

#include "Mapper.h"

#include <clang/CIR/Dialect/IR/CIRDialect.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdio>
#include <optional>

namespace cir2c {

namespace {

bool constantInt(mlir::Value v, int64_t &value) {
  auto c = v.getDefiningOp<cir::ConstantOp>();
  if (!c) return false;
  auto attr = mlir::dyn_cast<cir::IntAttr>(c.getValue());
  if (!attr) return false;
  value = attr.getSInt();
  return true;
}

// The element at flat index \p index of the vtable \p symbol, or null.
mlir::Attribute vtableEntryAt(mlir::ModuleOp module, const std::string &symbol, int64_t index) {
  if (index < 0) return {};
  auto global = module.lookupSymbol<cir::GlobalOp>(symbol);
  auto init = global ? global.getInitialValue() : std::nullopt;
  auto vtable = init ? mlir::dyn_cast<cir::VTableAttr>(*init) : cir::VTableAttr();
  if (!vtable) return {};
  for (mlir::Attribute component : vtable.getData()) {
    auto array = mlir::dyn_cast<cir::ConstArrayAttr>(component);
    auto elements = array ? mlir::dyn_cast<mlir::ArrayAttr>(array.getElts()) : mlir::ArrayAttr();
    if (!elements) return {};
    if (index < static_cast<int64_t>(elements.size())) return elements[index];
    index -= elements.size();
  }
  return {};
}

// The record type of a complete object: a local or global variable of a class
// type, whose vtable pointer is read directly.
cir::RecordType completeObjectRecord(mlir::Value object, mlir::ModuleOp module) {
  mlir::Type type;
  if (auto alloca = object.getDefiningOp<cir::AllocaOp>()) {
    type = alloca.getAllocaType();
  } else if (auto get = object.getDefiningOp<cir::GetGlobalOp>()) {
    if (auto ptr = mlir::dyn_cast<cir::PointerType>(get.getType())) type = ptr.getPointee();
  }
  auto record = mlir::dyn_cast_or_null<cir::RecordType>(type);
  if (!record || record.isUnion() || !record.getName()) return {};
  return record;
}

} // namespace

std::string Mapper::vtableIntegerEntry(mlir::Attribute entry) {
  auto constant = mlir::dyn_cast<cir::ConstPtrAttr>(entry);
  if (!constant || constant.isNullValue()) return "(void*)0";
  int64_t value = constant.getValue().getInt();
  if (value >= 0) return "(void*)" + std::to_string(value) + "UL";
  // An integer constant (C11 6.6p9): the two's complement as unsigned long.
  char hex[32];
  std::snprintf(hex, sizeof hex, "0x%llxUL", static_cast<unsigned long long>(value));
  return std::string("(void*)") + hex + " /* " + std::to_string(value) + " */";
}

std::optional<int64_t> Mapper::primaryVtableInteger(const std::string &className,
                                                    int64_t slot) const {
  auto it = primaryVtables_.find(className);
  if (it == primaryVtables_.end() || !vtableModule_) return std::nullopt;
  mlir::Attribute entry = vtableEntryAt(vtableModule_, it->second.first,
                                        static_cast<int64_t>(it->second.second) + slot);
  auto constant = mlir::dyn_cast_or_null<cir::ConstPtrAttr>(entry);
  if (!constant) return std::nullopt;
  return constant.isNullValue() ? 0 : constant.getValue().getInt();
}

void Mapper::planPrimaryVtables(mlir::ModuleOp module) {
  primaryVtables_.clear();
  vtableModule_ = nullptr;
  definedVtableClasses_.clear();
  // The classes whose vtable the module defines ("vtable for X").
  module->walk([&](cir::GlobalOp global) {
    auto sym = symbolNameAttr(global);
    if (!sym || !sym.getValue().starts_with("_ZTV") || !global.getInitialValue()) return;
    std::string dem = demangle(sym.getValue().str());
    const std::string prefix = "vtable for ";
    if (dem.rfind(prefix, 0) == 0) definedVtableClasses_.insert(dem.substr(prefix.size()));
  });
  std::set<std::string> ambiguous;
  module->walk([&](cir::StoreOp store) {
    auto point = store.getValue().getDefiningOp<cir::VTableAddrPointOp>();
    if (!point || point.getAddressPoint().getIndex() != 0) return;
    std::string symbol = point.getNameAttr().getValue().str();
    // A construction vtable (_ZTC) is in use only during the construction.
    if (symbol.rfind("_ZTV", 0) != 0) return;
    auto slot = store.getAddr().getDefiningOp<cir::VTableGetVPtrOp>();
    if (!slot) return;
    auto pointer = mlir::dyn_cast<cir::PointerType>(slot.getSrc().getType());
    auto record = pointer ? mlir::dyn_cast<cir::RecordType>(pointer.getPointee())
                          : cir::RecordType();
    if (!record || !record.getName()) return;
    std::string name = record.getName().getValue().str();
    uint64_t flat = vtableFlatOffset(point.getOperation(), symbol, 0,
                                     point.getAddressPoint().getOffset());
    auto it = primaryVtables_.find(name);
    if (it != primaryVtables_.end() && it->second != std::make_pair(symbol, flat))
      ambiguous.insert(name);
    primaryVtables_[name] = {symbol, flat};
    // The symbols are in the module of the operations, which can be nested in
    // the module that mapModule gets.
    if (!vtableModule_) vtableModule_ = store->getParentOfType<mlir::ModuleOp>();
  });
  for (const std::string &name : ambiguous) primaryVtables_.erase(name);
}

bool Mapper::startsVtableView(mlir::Operation *castOp) const {
  auto cast = mlir::dyn_cast<cir::CastOp>(castOp);
  if (!cast || cast.getKind() != cir::CastKind::bitcast) return false;
  mlir::Value src = cast.getSrc();
  return mlir::isa<cir::VPtrType>(src.getType()) || isVtableView(src);
}

bool Mapper::mapVtableViewCast(mlir::Operation *castOp) {
  auto cast = mlir::cast<cir::CastOp>(castOp);
  mlir::Value src = cast.getSrc();
  auto it = vtableViews_.find(src);
  VtableView view = it != vtableViews_.end() ? it->second : VtableView{src, 0};
  vtableViews_[cast.getResult()] = view;
  return true;
}

bool Mapper::mapVtableViewStride(mlir::Operation *strideOp) {
  mlir::Value base = strideOp->getOperand(0);
  VtableView view = vtableViews_.at(base);
  int64_t count = 0;
  auto pointer = mlir::dyn_cast<cir::PointerType>(base.getType());
  if (!constantInt(strideOp->getOperand(1), count) || !pointer || !dataLayout_) {
    llvm::errs() << "cir2c: error: a vtable is indexed with a stride that is not a constant "
                    "(issue #3)\n";
    return false;
  }
  view.offset += count * static_cast<int64_t>(dataLayout_->getTypeSize(pointer.getPointee()));
  vtableViews_[strideOp->getResult(0)] = view;
  return true;
}

bool Mapper::mapVtableViewLoad(mlir::Operation *loadOp, std::ostream &out) {
  VtableView view = vtableViews_.at(loadOp->getOperand(0));
  mlir::Value result = loadOp->getResult(0);
  auto integer = mlir::dyn_cast<cir::IntType>(result.getType());
  bool isPointer = mlir::isa<cir::PointerType>(result.getType());
  if (view.offset % 8 != 0 || !((integer && integer.getWidth() == 64) || isPointer)) {
    llvm::errs() << "cir2c: error: a read of " << view.offset
                 << " bytes from a vtable address point that is not a 64-bit integer or a "
                    "pointer at a slot (issue #3)\n";
    return false;
  }
  int64_t slot = view.offset / 8;
  std::string ctype = mapTypeToC(result.getType());
  std::string name = freshName("vt");
  setName(result, name);

  if (integer) {
    // A complete object: the entry of the primary vtable of its class.
    auto vptrLoad = view.vptr.getDefiningOp<cir::LoadOp>();
    auto slotOp = vptrLoad ? vptrLoad.getAddr().getDefiningOp<cir::VTableGetVPtrOp>()
                           : cir::VTableGetVPtrOp();
    mlir::ModuleOp module = loadOp->getParentOfType<mlir::ModuleOp>();
    if (slotOp && module) {
      if (auto record = completeObjectRecord(slotOp.getSrc(), module)) {
        auto it = primaryVtables_.find(record.getName().getValue().str());
        if (it != primaryVtables_.end()) {
          mlir::Attribute entry = vtableEntryAt(module, it->second.first,
                                                static_cast<int64_t>(it->second.second) + slot);
          auto constant = mlir::dyn_cast_or_null<cir::ConstPtrAttr>(entry);
          if (constant) {
            out << "  " << ctype << " " << name << " = " << constant.getValue().getInt()
                << "L; /* vtable entry " << slot << " of a complete object */\n";
            return true;
          }
        }
      }
    }
    // A library class whose vtable is not in the module (issue #32): libc++
    // instantiates its streams in its compiled library. No code of the output
    // sets the vptr of such an object, in either mode, so its entries cannot
    // be read. In --externalize-std, a read whose only use is a replaced call
    // emits nothing (planReplacedCallArguments) and does not come here.
    if (slotOp) {
      auto pointer = mlir::dyn_cast<cir::PointerType>(slotOp.getSrc().getType());
      auto record = pointer ? mlir::dyn_cast<cir::RecordType>(pointer.getPointee())
                            : cir::RecordType();
      if (record && record.getName() && record.getName().getValue().starts_with("std::") &&
          !definedVtableClasses_.count(record.getName().getValue().str())) {
        llvm::errs() << "cir2c: error: the vtable of the library class "
                     << record.getName().getValue()
                     << " is not in the module, so no code of the output sets the vtable pointer "
                        "of this object, and its vtable entries cannot be read (issue #32)\n";
        return false;
      }
    }
    out << "  " << ctype << " " << name << " = (" << ctype << ")(unsigned long)((void**)"
        << getOrCreateName(view.vptr) << ")[" << slot << "];\n";
    return true;
  }
  out << "  " << ctype << " " << name << " = (" << ctype << ")((void**)"
      << getOrCreateName(view.vptr) << ")[" << slot << "];\n";
  return true;
}

} // namespace cir2c

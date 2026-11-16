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

#include "handlers/HandlerModule.h"
#include "handlers/HandlerSupport.h"
#include "Mapper.h"

#include <mlir/IR/Operation.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>
#include <clang/CIR/Dialect/IR/CIRDialect.h>

#include <string>
#include <cctype>
#include <sstream>

using namespace mlir;

namespace cir2c {

/// Handlers for VTable / virtual dispatch operations and VTT / derived-class
/// address operations.
class VTableHandlers : public HandlerModule {
public:
  void registerHandlers(Mapper &m) override {
    // VTable / virtual dispatch
    m.registerTypedHandler<cir::VTableAddrPointOp>(handleVTableAddrPoint);
    m.registerTypedHandler<cir::VTableGetVPtrOp>(handleVTableGetVPtr);
    m.registerTypedHandler<cir::VTableGetVirtualFnAddrOp>(handleVTableGetVirtualFnAddr);
    m.registerTypedHandler<cir::VTableGetTypeInfoOp>(handleVTableGetTypeInfo);
    m.registerTypedHandler<cir::VTTAddrPointOp>(handleVTTAddrPoint);
    m.registerHandler("cir.derived_class_addr", std::make_unique<LambdaOpHandler>(handleDerivedClassAddr));
  }

private:
  // cir.vtable.address_point(@_ZTV, <index, offset>): the value installed into an
  // object's __vptr field by a constructor. The vtable is emitted as a flat
  // `void* _ZTV[]` array, so the address point is `&_ZTV[flat]`, where `flat` is
  // the element offset (for multiple inheritance, the size of the preceding
  // sub-vtable components plus `offset`). The result is a vptr (mapped to void*).
  static bool handleVTableAddrPoint(cir::VTableAddrPointOp op, Mapper &m, std::ostream &out) {
    if (op->getNumResults() < 1) return false;
    std::string sym = Mapper::sanitizeIdentifier(op.getNameAttr().getValue().str());
    cir::AddressPointAttr ap = op.getAddressPoint();
    uint64_t flat = m.vtableFlatOffset(op.getOperation(),
                                       op.getNameAttr().getValue().str(),
                                       ap.getIndex(), ap.getOffset());
    std::string name = m.getOrCreateName(op->getResult(0));
    out << "  void* " << name << " = (void*)&" << sym << "[" << flat << "];\n";
    return true;
  }

  // cir.vtable.get_vptr: retrieves the address of the __vptr field of an
  // object.  We register the result in the dispatch chain and emit nothing.
  static bool handleVTableGetVPtr(cir::VTableGetVPtrOp op, Mapper &m, std::ostream &out) {
    if (op->getNumOperands() > 0 && op->getNumResults() > 0) {
      m.trackVtableDispatch(op->getResult(0), op.getSrc());
      std::string name = m.getOrCreateName(op->getResult(0));
      std::string src  = m.getOrCreateName(op.getSrc());
      // Source must be a pointer expression. If it's a direct-access lvalue
      // (e.g. a local struct), take its address first.
      if (m.isDirectAccess(op.getSrc()) && !src.empty() && src[0] != '&')
        src = "&(" + src + ")";
      // The result is the address of the __vptr field; type !cir.ptr<!cir.vptr>
      // maps to void** (the slot holds a void* vtable pointer). The vptr is at
      // offset 0 of the struct, so its address equals the struct address.
      out << "  void** " << name << " = (void**)" << src << ";\n";
    }
    return true;
  }

  // cir.vtable.get_virtual_fn_addr: indexes into the vtable to get the address of
  // a specific virtual function pointer slot. We extend the dispatch chain and
  // record the slot index so handleCall can pass it to __VERIFIER_virtual_call.
  // Emits nothing (the slot is consumed by the eventual call).
  static bool handleVTableGetVirtualFnAddr(cir::VTableGetVirtualFnAddrOp op, Mapper &m, std::ostream &out) {
    if (op->getNumResults() > 0) {
      Value vptr   = op.getVptr();
      Value result = op->getResult(0);
      mlir::Value chainRoot = m.isVtableDispatchValue(vptr)
                                  ? m.getVtableDispatchObject(vptr)
                                  : vptr;
      m.trackVtableDispatch(result, chainRoot);
      m.getOrCreateName(result);
      m.setVirtualFnSlot(result, static_cast<int>(op.getIndex()));
    }
    return true;
  }

  // cir.vtable.get_type_info: retrieves the type_info pointer from a vtable.
  // For verification we emit a null-pointer placeholder.
  static bool handleVTableGetTypeInfo(cir::VTableGetTypeInfoOp op, Mapper &m, std::ostream &out) {
    if (op->getNumResults() > 0) {
      std::string tmp   = m.freshName("tinfo");
      std::string ctype = m.mapTypeToC(op->getResult(0).getType());
      out << "  " << ctype << " " << tmp << " = (" << ctype << ")0;\n";
      m.setName(op->getResult(0), tmp);
    }
    return true;
  }

  // cir.vtt.address_point: an element of a VTT (issue #3). The complete-object
  // constructor of a class with a virtual base gives each base constructor a
  // part of its VTT, the `void *` array of the vtable pointers that the base
  // constructor installs: `@_ZTT, offset = k` is &_ZTT[k], and `%vtt, offset = k`
  // is element k of the part that the constructor got.
  static bool handleVTTAddrPoint(cir::VTTAddrPointOp op, Mapper &m, std::ostream &out) {
    if (op->getNumResults() < 1) return false;
    std::string ctype = m.mapTypeToC(op->getResult(0).getType());
    std::string tmp = m.freshName("vtt");
    std::string offset = std::to_string(op.getOffset());
    if (auto name = op.getNameAttr()) {
      out << "  " << ctype << " " << tmp << " = (" << ctype << ")&"
          << Mapper::sanitizeIdentifier(name.getValue().str()) << "[" << offset << "];\n";
    } else if (op.getSymAddr()) {
      out << "  " << ctype << " " << tmp << " = (" << ctype << ")((void**)"
          << m.getOrCreateName(op.getSymAddr()) << " + " << offset << ");\n";
    } else {
      return false;
    }
    m.setName(op->getResult(0), tmp);
    return true;
  }

  // cir.derived_class_addr is the inverse of cir.base_class_addr: given a base
  // pointer it returns a pointer to the enclosing derived object by going
  // backwards in memory by the given offset (lowering to a negative offset).
  // Mirror handleBaseClassAddr but subtract the offset instead of adding it.
  static bool handleDerivedClassAddr(Operation *o, Mapper &m, std::ostream &out) {
    if (o->getNumOperands() < 1 || o->getNumResults() < 1) return false;

    Value base = o->getOperand(0);
    std::string baseName = m.getOrCreateName(base);
    if (m.isDirectAccess(base) && !baseName.empty() && baseName[0] != '&') {
      baseName = "&(" + baseName + ")";
    }
    std::string resultType = m.mapTypeToC(o->getResult(0).getType());

    std::string opText;
    llvm::raw_string_ostream rso(opText);
    o->print(rso, mlir::OpPrintingFlags().assumeVerified().useLocalScope());
    rso.flush();

    bool isNonnull = opText.find(" nonnull ") != std::string::npos;
    long long offset = 0;
    size_t openBracket = opText.rfind('[');
    size_t closeBracket = (openBracket == std::string::npos) ? std::string::npos : opText.find(']', openBracket + 1);
    if (openBracket != std::string::npos && closeBracket != std::string::npos && closeBracket > openBracket + 1) {
      std::string offsetText = opText.substr(openBracket + 1, closeBracket - openBracket - 1);
      size_t start = 0;
      while (start < offsetText.size() && std::isspace(static_cast<unsigned char>(offsetText[start]))) ++start;
      size_t end = offsetText.size();
      while (end > start && std::isspace(static_cast<unsigned char>(offsetText[end - 1]))) --end;
      if (end > start) {
        offset = std::stoll(offsetText.substr(start, end - start));
      }
    }

    std::string adjustedExpr = "(" + resultType + ")((char *)" + baseName + " - " + std::to_string(offset) + ")";
    if (!isNonnull) {
      adjustedExpr = "((" + baseName + ") ? " + adjustedExpr + " : (" + resultType + ")0)";
    }

    std::string tmp = m.freshName("derived");
    out << "  " << resultType << " " << tmp << " = " << adjustedExpr << ";\n";
    m.setName(o->getResult(0), tmp);
    return true;
  }
};

REGISTER_HANDLER_MODULE(VTableHandlers)

} // namespace cir2c

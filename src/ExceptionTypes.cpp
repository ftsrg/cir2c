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

// The selection of an exception handler (issue #2, C++ [except.handle]/3).
//
// A handler of type H matches an exception of type E when E is H, when H is an
// unambiguous public base class of E, or when H and E are pointer types and E
// converts to H by a qualification conversion, a conversion to `void *` or a
// conversion to a pointer to an unambiguous public base class.
//
// cir2c decides each pair statically. The RTTI objects of the CIR give the
// class hierarchy: a class without bases (__class_type_info), one public
// non-virtual base at offset 0 (__si_class_type_info), or a list of bases with
// their offsets and flags (__vmi_class_type_info). The RTTI of a pointer type
// (__pointer_type_info) gives the qualifiers and the type it points to. The
// RTTI of a fundamental type is in the C++ runtime, so its mangled name gives
// the same information. The thrown types are those of the cir.throw operations
// of the module.
//
// For each handler type, the output gets one function that compares the type
// tag of the exception with the tags of the matching types. It sets
// __cir_exc_adj, the address of the matching subobject, which the handler
// binds. For a pointer handler, it also sets __cir_exc_pval, the converted
// pointer value. __cir_exc_ptr and __cir_exc_type keep the exception object
// and its type, for a rethrow and for the destructor at the end of the handler.

#include "Mapper.h"

#include <clang/CIR/Dialect/IR/CIRDialect.h>
#include <llvm/Support/raw_ostream.h>

#include <functional>
#include <set>

namespace cir2c {

namespace {

// The RTTI of a fundamental pointer type is in the C++ runtime. Its mangled
// name is `_ZTIP` followed by the qualifiers of the pointee (r, V, K) and the
// mangled pointee type.
bool parseExternalPointer(const std::string &sym, std::string &pointee, unsigned &quals) {
  if (sym.rfind("_ZTIP", 0) != 0) return false;
  size_t i = 5;
  quals = 0;
  for (; i < sym.size(); ++i) {
    if (sym[i] == 'K') quals |= 1;
    else if (sym[i] == 'V') quals |= 2;
    else if (sym[i] == 'r') quals |= 4;
    else break;
  }
  if (i >= sym.size()) return false;
  pointee = "_ZTI" + sym.substr(i);
  return true;
}

// True when the RTTI symbol is that of a function type (pointee of a pointer to
// a function), which does not convert to `void *`.
bool isFunctionTypeInfo(const std::string &sym) {
  return sym.rfind("_ZTIF", 0) == 0 || sym.rfind("_ZTIDo", 0) == 0;
}

} // namespace

void Mapper::planExceptionTypes(mlir::ModuleOp module) {
  module->walk([&](cir::GlobalOp global) {
    auto symAttr = symbolNameAttr(global);
    if (!symAttr) return;
    std::string sym = symAttr.getValue().str();
    if (sym.rfind("_ZTI", 0) != 0) return;
    EhTypeInfo info;
    auto init = global.getInitialValue();
    auto typeInfo = init ? mlir::dyn_cast<cir::TypeInfoAttr>(*init) : cir::TypeInfoAttr();
    if (!typeInfo) {
      if (parseExternalPointer(sym, info.pointee, info.quals)) info.kind = EhTypeInfo::Pointer;
      ehTypeInfos_[sym] = info;
      return;
    }
    mlir::ArrayAttr data = typeInfo.getData();
    auto symbolAt = [&](size_t i) -> std::string {
      if (i >= data.size()) return std::string();
      auto view = mlir::dyn_cast<cir::GlobalViewAttr>(data[i]);
      return view ? view.getSymbol().getValue().str() : std::string();
    };
    auto intAt = [&](size_t i, int64_t &value) {
      if (i >= data.size()) return false;
      auto attr = mlir::dyn_cast<cir::IntAttr>(data[i]);
      if (!attr) return false;
      value = attr.getSInt();
      return true;
    };
    std::string vtable = symbolAt(0);
    if (vtable.find("17__class_type_info") != std::string::npos) {
      info.kind = EhTypeInfo::Class;
    } else if (vtable.find("20__si_class_type_info") != std::string::npos) {
      info.kind = EhTypeInfo::Class;
      info.bases.push_back({symbolAt(2), 0, true, false});
    } else if (vtable.find("21__vmi_class_type_info") != std::string::npos) {
      info.kind = EhTypeInfo::Class;
      int64_t count = 0;
      if (!intAt(3, count)) {
        info.kind = EhTypeInfo::Unknown;
      } else {
        for (int64_t k = 0; k < count; ++k) {
          int64_t flags = 0;
          std::string base = symbolAt(4 + 2 * k);
          if (base.empty() || !intAt(5 + 2 * k, flags)) {
            info.kind = EhTypeInfo::Unknown;
            break;
          }
          // __offset_flags: the offset in the upper bits, 2 public, 1 virtual.
          info.bases.push_back({base, flags >> 8, (flags & 2) != 0, (flags & 1) != 0});
        }
      }
    } else if (vtable.find("19__pointer_type_info") != std::string::npos) {
      int64_t flags = 0;
      info.pointee = symbolAt(3);
      if (intAt(2, flags) && !info.pointee.empty()) {
        info.kind = EhTypeInfo::Pointer;
        info.quals = static_cast<unsigned>(flags) & 7;
      }
    }
    ehTypeInfos_[sym] = info;
  });
  module->walk([&](cir::ThrowOp op) {
    auto ti = op.getTypeInfoAttr();
    if (!ti || op.rethrows() || !op.getExceptionPtr()) return;
    auto pointer = mlir::dyn_cast<cir::PointerType>(op.getExceptionPtr().getType());
    if (pointer) ehThrownTypes_.emplace(ti.getValue().str(), pointer.getPointee());
  });
}

Mapper::EhMatch Mapper::matchClass(const std::string &thrown, const std::string &handler) const {
  EhMatch result;
  if (thrown == handler) {
    result.matches = true;
    return result;
  }
  // Each path from the thrown class to a base subobject of the handler class.
  std::set<int64_t> offsets;
  std::set<int64_t> publicOffsets;
  bool throughVirtual = false;
  std::function<void(const std::string &, int64_t, bool, int)> walk =
      [&](const std::string &cls, int64_t offset, bool allPublic, int depth) {
        auto it = ehTypeInfos_.find(cls);
        if (it == ehTypeInfos_.end() || depth > 64) return;
        for (const EhBase &base : it->second.bases) {
          if (base.isVirtual) {
            // The offset of a virtual base is in the vtable of the object.
            std::function<bool(const std::string &, int)> reaches =
                [&](const std::string &c, int d) {
                  if (c == handler) return true;
                  auto bi = ehTypeInfos_.find(c);
                  if (bi == ehTypeInfos_.end() || d > 64) return false;
                  for (const EhBase &b : bi->second.bases)
                    if (reaches(b.sym, d + 1)) return true;
                  return false;
                };
            if (reaches(base.sym, depth + 1)) throughVirtual = true;
            continue;
          }
          int64_t at = offset + base.offset;
          bool isPublic = allPublic && base.isPublic;
          if (base.sym == handler) {
            offsets.insert(at);
            if (isPublic) publicOffsets.insert(at);
            continue;
          }
          walk(base.sym, at, isPublic, depth + 1);
        }
      };
  walk(thrown, 0, true, 0);
  if (throughVirtual) {
    result.unsupported = true;
    return result;
  }
  // Two subobjects of one type have different addresses, so the offsets
  // count the subobjects. More than one is ambiguous: no match.
  if (offsets.size() == 1 && publicOffsets.size() == 1) {
    result.matches = true;
    result.offset = *offsets.begin();
  }
  return result;
}

Mapper::EhMatch Mapper::matchHandler(const std::string &thrown, const std::string &handler) const {
  EhMatch none;
  if (thrown == handler) {
    EhMatch exact;
    exact.matches = true;
    return exact;
  }
  auto h = ehTypeInfos_.find(handler);
  auto e = ehTypeInfos_.find(thrown);
  if (h == ehTypeInfos_.end()) return none;
  const EhTypeInfo &hi = h->second;
  if (hi.kind == EhTypeInfo::Class) {
    if (e == ehTypeInfos_.end() || e->second.kind != EhTypeInfo::Class) return none;
    return matchClass(thrown, handler);
  }
  if (hi.kind != EhTypeInfo::Pointer) return none;
  // std::nullptr_t converts to each pointer type.
  if (thrown == "_ZTIDn") {
    EhMatch null;
    null.matches = true;
    null.nullValue = true;
    return null;
  }
  if (e == ehTypeInfos_.end() || e->second.kind != EhTypeInfo::Pointer) return none;
  const EhTypeInfo &ei = e->second;
  // A qualification conversion adds qualifiers to the pointee.
  if ((ei.quals & ~hi.quals) != 0) return none;
  EhMatch result;
  result.converts = true;
  if (ei.pointee == hi.pointee) {
    result.matches = true;
    return result;
  }
  if (hi.pointee == "_ZTIv") {
    result.matches = !isFunctionTypeInfo(ei.pointee);
    return result;
  }
  auto ep = ehTypeInfos_.find(ei.pointee);
  auto hp = ehTypeInfos_.find(hi.pointee);
  if (ep == ehTypeInfos_.end() || hp == ehTypeInfos_.end() ||
      ep->second.kind != EhTypeInfo::Class || hp->second.kind != EhTypeInfo::Class)
    return none;
  EhMatch base = matchClass(ei.pointee, hi.pointee);
  base.converts = true;
  return base;
}

bool Mapper::exceptionMatchCall(const std::string &handler, std::string &call) {
  std::string san = sanitizeIdentifier(handler);
  call = "__cir_eh_match_" + san + "()";
  if (ehMatchersPlanned_.count(handler)) return true;
  // Check the pairs now, so that an unsupported pair stops the translation.
  for (const auto &thrown : ehThrownTypes_) {
    EhMatch match = matchHandler(thrown.first, handler);
    if (match.unsupported) {
      llvm::errs() << "cir2c: error: the handler of " << demangle(handler)
                   << " can match the exception " << demangle(thrown.first)
                   << " through a virtual base class, which cir2c does not translate "
                      "(issue #3)\n";
      return false;
    }
  }
  ehMatchersPlanned_.insert(handler);
  ehMatchersPending_.push_back(handler);
  verifierDeclsBuf_ << "static int __cir_eh_match_" << san << "(void);\n";
  return true;
}

bool Mapper::handlerMatchesOnlyItself(const std::string &handler) const {
  for (const auto &thrown : ehThrownTypes_) {
    if (thrown.first == handler) continue;
    EhMatch match = matchHandler(thrown.first, handler);
    if (match.matches || match.unsupported) return false;
  }
  return true;
}

void Mapper::emitExceptionMatchers(std::ostream &out) {
  if (ehMatchersPending_.empty()) return;
  out << "// Handler selection (C++ [except.handle]/3): the exception types that\n"
         "// each handler type matches. __cir_exc_adj is the address of the\n"
         "// matching subobject; for a pointer handler, __cir_exc_pval is the\n"
         "// converted pointer.\n";
  for (const std::string &handler : ehMatchersPending_) {
    auto hi = ehTypeInfos_.find(handler);
    bool pointerHandler = hi != ehTypeInfos_.end() && hi->second.kind == EhTypeInfo::Pointer;
    out << "static int __cir_eh_match_" << sanitizeIdentifier(handler) << "(void) {\n";
    // The handler type itself first, also when the module throws it nowhere.
    std::vector<std::string> candidates = {handler};
    for (const auto &thrown : ehThrownTypes_)
      if (thrown.first != handler) candidates.push_back(thrown.first);
    for (const std::string &thrown : candidates) {
      EhMatch match = matchHandler(thrown, handler);
      if (!match.matches) continue;
      std::string body;
      if (!pointerHandler) {
        body = match.offset == 0
                   ? "__cir_exc_adj = __cir_exc_ptr;"
                   : "__cir_exc_adj = (void*)((char*)__cir_exc_ptr + " +
                         std::to_string(match.offset) + ");";
      } else if (match.nullValue) {
        body = "__cir_exc_adj = (void*)0; __cir_exc_pval = (void*)0;";
      } else {
        auto type = ehThrownTypes_.find(thrown);
        // The value of a pointer that the module does not throw is unknown.
        if (type == ehThrownTypes_.end()) continue;
        std::string ctype = mapTypeToC(type->second);
        // An exact match binds a reference to the exception object itself.
        std::string adj = (thrown == handler) ? "__cir_exc_adj = __cir_exc_ptr; "
                                              : "__cir_exc_adj = (void*)0; ";
        if (match.offset == 0) {
          body = adj + "__cir_exc_pval = (void*)*(" + ctype + "*)__cir_exc_ptr;";
        } else {
          body = "{ " + ctype + " v = *(" + ctype + "*)__cir_exc_ptr; " + adj +
                 "__cir_exc_pval = v ? (void*)((char*)v + " + std::to_string(match.offset) +
                 ") : (void*)0; }";
        }
      }
      out << "  if (__cir_exc_type == (const void*)__cir_eh_type_" << sanitizeIdentifier(thrown)
          << ") { " << body << " return 1; }\n";
    }
    out << "  return 0;\n}\n";
  }
  out << "\n";
  ehMatchersPending_.clear();
}

bool Mapper::rejectVirtualBases(mlir::ModuleOp module) {
  // The classes with a virtual base, directly or through a base class.
  std::function<bool(const std::string &, int)> hasVirtualBase =
      [&](const std::string &cls, int depth) {
        auto it = ehTypeInfos_.find(cls);
        if (it == ehTypeInfos_.end() || depth > 64) return false;
        for (const EhBase &base : it->second.bases)
          if (base.isVirtual || hasVirtualBase(base.sym, depth + 1)) return true;
        return false;
      };
  std::string what;
  module->walk([&](cir::GlobalOp global) {
    if (!what.empty()) return;
    auto symAttr = symbolNameAttr(global);
    if (!symAttr) return;
    std::string sym = symAttr.getValue().str();
    if (globalElided(sym)) return;
    if (sym.rfind("_ZTT", 0) == 0 || sym.rfind("_ZTC", 0) == 0) {
      what = demangle(sym);
      return;
    }
    if (sym.rfind("_ZTV", 0) != 0) return;
    auto init = global.getInitialValue();
    auto vtable = init ? mlir::dyn_cast<cir::VTableAttr>(*init) : cir::VTableAttr();
    if (!vtable) return;
    // The RTTI entries of the vtable name its class.
    vtable.walk([&](cir::GlobalViewAttr view) {
      std::string target = view.getSymbol().getValue().str();
      if (what.empty() && target.rfind("_ZTI", 0) == 0 && hasVirtualBase(target, 0))
        what = demangle(sym);
    });
  });
  module->walk([&](cir::VTTAddrPointOp op) {
    if (!what.empty()) return;
    auto func = op->getParentOfType<cir::FuncOp>();
    auto funcSym = func ? symbolNameAttr(func) : mlir::StringAttr();
    if (!funcSym || !funcDefElided(funcSym.getValue()))
      what = "a VTT in " + (funcSym ? demangle(funcSym.getValue()) : std::string("a global"));
  });
  if (what.empty()) return true;
  llvm::errs() << "cir2c: error: the program uses a virtual base class (" << what
               << "). cir2c does not translate virtual bases yet: the vtables have no "
                  "virtual-base offsets (issue #3).\n";
  return false;
}

} // namespace cir2c

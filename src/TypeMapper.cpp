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

#include "TypeMapper.h"
#include "Mapper.h"

#include <mlir/IR/Types.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/BuiltinAttributes.h>

#include <clang/CIR/Dialect/IR/CIRDialect.h>

#include <llvm/ADT/DenseSet.h>
#include <llvm/ADT/StringRef.h>

#include <algorithm>
#include <functional>
#include <vector>

using namespace mlir;

namespace cir2c {

// Clang names an unnamed class by its source location, for example
// "(lambda at /home/user/llvm/include/c++/v1/list:1276:48)". The directory is
// that of the machine that ran clang, so a name keeps only the last `depth`
// components of the path: with depth 1, only the file name.
static std::string keepLocationTail(llvm::StringRef raw, unsigned depth) {
  std::string out;
  size_t pos = 0;
  while (true) {
    size_t at = raw.find(" at ", pos);
    if (at == llvm::StringRef::npos) break;
    size_t close = raw.find(')', at);
    if (close == llvm::StringRef::npos) break;
    llvm::StringRef location = raw.slice(at + 4, close);
    size_t cut = llvm::StringRef::npos;
    unsigned slashes = 0;
    for (size_t i = location.size(); i-- > 0;)
      if (location[i] == '/' && ++slashes == depth) {
        cut = i;
        break;
      }
    out += raw.slice(pos, at + 4).str();
    out += (cut == llvm::StringRef::npos ? location : location.drop_front(cut + 1)).str();
    pos = close;
  }
  out += raw.drop_front(pos).str();
  return out;
}

// The number of path components of the longest source location in `raw`.
static unsigned locationDepth(llvm::StringRef raw) {
  unsigned depth = 0;
  for (size_t at = raw.find(" at "); at != llvm::StringRef::npos;
       at = raw.find(" at ", at + 4)) {
    size_t close = raw.find(')', at);
    if (close == llvm::StringRef::npos) break;
    depth = std::max<unsigned>(depth, raw.slice(at + 4, close).count('/') + 1);
  }
  return depth;
}

// Strip CIR's ".base" suffix before sanitizing a record type name.
// CIR emits "X.base" as the internal name for the base subobject layout of
// class X when it is embedded in a derived class. Stripping it ensures the
// generated C reuses the same struct tag as the complete class, which is
// correct because: (a) the '.' character is impossible in any user-defined
// C++ class name so there is no ambiguity, and (b) the base subobject layout
// is layout-compatible with the complete type for verification purposes.
static llvm::StringRef withoutBase(llvm::StringRef raw) {
  return raw.ends_with(".base") ? raw.drop_back(5) : raw;
}

/*static*/
std::string TypeMapper::plainCName(llvm::StringRef raw) {
  return Mapper::sanitizeIdentifier(keepLocationTail(withoutBase(raw), 1));
}

std::string TypeMapper::recordCName(mlir::StringAttr nameAttr) const {
  if (!nameAttr || nameAttr.getValue().empty()) return "anon_struct";
  return recordCName(nameAttr.getValue());
}

std::string TypeMapper::recordCName(llvm::StringRef raw) const {
  std::string key = withoutBase(raw).str();
  auto it = recordCNames_.find(key);
  if (it != recordCNames_.end()) return it->second;
  // A record that planRecordNames did not see gets a free tag now.
  std::string base = plainCName(key), name = base;
  for (unsigned n = 2; usedRecordCNames_.count(name); ++n)
    name = base + "_" + std::to_string(n);
  usedRecordCNames_.insert(name);
  recordCNames_[key] = name;
  return name;
}

void TypeMapper::planRecordNames(mlir::ModuleOp module) {
  recordCNames_.clear();
  usedRecordCNames_.clear();

  // Every record name of the module: in the types of the values and of the
  // attributes (function types, the types of globals, initializers), and in
  // the members of the records found.
  std::set<std::string> names;
  llvm::DenseSet<mlir::Type> seen;
  std::function<void(mlir::Type)> visit = [&](mlir::Type t) {
    if (!t || !seen.insert(t).second) return;
    if (auto rec = mlir::dyn_cast<cir::RecordType>(t)) {
      if (rec.getName() && !rec.getName().getValue().empty())
        names.insert(withoutBase(rec.getName().getValue()).str());
      if (rec.isComplete())
        for (mlir::Type member : rec.getMembers()) visit(member);
    } else if (auto ptr = mlir::dyn_cast<cir::PointerType>(t)) {
      visit(ptr.getPointee());
    } else if (auto arr = mlir::dyn_cast<cir::ArrayType>(t)) {
      visit(arr.getElementType());
    } else if (auto fn = mlir::dyn_cast<cir::FuncType>(t)) {
      for (mlir::Type input : fn.getInputs()) visit(input);
      visit(fn.getReturnType());
    } else if (auto cx = mlir::dyn_cast<cir::ComplexType>(t)) {
      visit(cx.getElementType());
    }
  };
  module->walk([&](mlir::Operation *op) {
    for (mlir::Type t : op->getResultTypes()) visit(t);
    for (mlir::Type t : op->getOperandTypes()) visit(t);
    for (mlir::Region &region : op->getRegions())
      for (mlir::Block &block : region)
        for (mlir::BlockArgument arg : block.getArguments()) visit(arg.getType());
    op->getAttrDictionary().walk([&](mlir::Type t) { visit(t); });
  });

  // Group the names by their plain tag. A name alone in its group keeps it.
  std::map<std::string, std::vector<std::string>> groups;
  for (const std::string &name : names) groups[plainCName(name)].push_back(name);
  for (auto &[tag, group] : groups)
    if (group.size() == 1) {
      recordCNames_[group.front()] = tag;
      usedRecordCNames_.insert(tag);
    }
  for (auto &[tag, group] : groups) {
    if (group.size() == 1) continue;
    // Names that differ only in the directories of their source locations
    // keep more components of the path, until they differ. The tail of a
    // path does not depend on the machine.
    std::vector<std::string> chosen;
    unsigned maxDepth = 0;
    for (const std::string &name : group)
      maxDepth = std::max(maxDepth, locationDepth(name));
    for (unsigned depth = 2; depth <= maxDepth && chosen.empty(); ++depth) {
      std::vector<std::string> candidate;
      std::set<std::string> distinct;
      for (const std::string &name : group) {
        candidate.push_back(Mapper::sanitizeIdentifier(keepLocationTail(name, depth)));
        distinct.insert(candidate.back());
      }
      bool free = distinct.size() == group.size();
      for (const std::string &c : candidate) free = free && !usedRecordCNames_.count(c);
      if (free) chosen = candidate;
    }
    // Other names, for example `ns::X` and `ns__X`: a numeric suffix in the
    // order of the names, as for functions with one name.
    if (chosen.empty())
      for (size_t i = 0; i < group.size(); ++i) {
        std::string name = tag;
        for (unsigned n = 2; usedRecordCNames_.count(name) ||
                             std::count(chosen.begin(), chosen.end(), name);
             ++n)
          name = tag + "_" + std::to_string(n);
        chosen.push_back(name);
      }
    for (size_t i = 0; i < group.size(); ++i) {
      recordCNames_[group[i]] = chosen[i];
      usedRecordCNames_.insert(chosen[i]);
    }
  }
}

std::string TypeMapper::anonRecordCName(mlir::Type recordType) const {
  auto it = anonRecordNames_.find(recordType);
  if (it != anonRecordNames_.end())
    return it->second;
  std::string name = "anon_struct_" + std::to_string(anonRecordNames_.size());
  anonRecordNames_[recordType] = name;
  return name;
}

std::string TypeMapper::mapTypeToC(mlir::Type t) const {
  // Handle MLIR built-in integer types
  if (auto it = mlir::dyn_cast<mlir::IntegerType>(t)) {
    unsigned w = it.getWidth();
    switch (w) {
    case 1:
      return "bool";
    case 8:
      return "signed char";
    case 16:
      return "short";
    case 32:
      return "int";
    case 64:
      return "long";
    default:
      return "long";
    }
  }

  // Handle MLIR built-in float types
  if (auto ft = mlir::dyn_cast<mlir::FloatType>(t)) {
    unsigned w = ft.getWidth();
    switch (w) {
    case 16:
      return "float"; // half precision, approximate as float
    case 32:
      return "float";
    case 64:
      return "double";
    case 80:
      return "long double"; // x87 extended precision
    case 128:
      return "long double"; // quad precision, approximate as long double
    default:
      return "double";
    }
  }

  // Handle MLIR NoneType (sometimes used for void)
  if (mlir::isa<mlir::NoneType>(t)) {
    return "void";
  }

  // Handle CIR void type
  if (mlir::isa<cir::VoidType>(t)) {
    return "void";
  }

  // Handle CIR bool type
  if (mlir::isa<cir::BoolType>(t)) {
    return "_Bool";
  }

  // Handle CIR integer types
  if (auto intTy = mlir::dyn_cast<cir::IntType>(t)) {
    unsigned width = intTy.getWidth();
    bool isSigned = intTy.isSigned();

    if (width == 8) {
      return isSigned ? "char" : "unsigned char";
    } else if (width == 16) {
      return isSigned ? "short" : "unsigned short";
    } else if (width == 32) {
      return isSigned ? "int" : "unsigned int";
    } else if (width == 64) {
      return isSigned ? "long" : "unsigned long";
    }

    // Non-standard widths. Upstream coerces a by-value record into an integer
    // of exactly the record's width, so a 6-byte packed struct arrives as
    // !cir.int<u, 48>. C has no 48-bit type, so the value has to travel in the
    // next standard type up — never a narrower one.
    //
    // The fallback below used to be a flat "int", which is 32 bits: every
    // width above 32 was silently truncated, and a struct passed by value lost
    // its top bytes. Widths at or below 32 keep the historical "int" so this
    // stays a fix for the truncating cases only.
    if (width > 32 && width <= 64)
      return isSigned ? "long" : "unsigned long";
    return "int"; // fallback (widths <= 32; wider than needed but lossless)
  }

  // Handle CIR floating-point types
  if (mlir::isa<cir::SingleType>(t)) {
    return "float";
  }
  if (mlir::isa<cir::DoubleType>(t)) {
    return "double";
  }
  if (mlir::isa<cir::LongDoubleType>(t)) {
    return "long double";
  }
  if (mlir::isa<cir::FP80Type>(t)) {
    return "long double";
  }
  if (mlir::isa<cir::FP16Type>(t)) {
    return "_Float16";
  }
  if (mlir::isa<cir::BF16Type>(t)) {
    return "__bf16";
  }

  // CIR complex type -> C99 `_Complex`. Element type drives the base (e.g.
  // !cir.complex<!cir.double> becomes "double _Complex").
  if (auto cplx = mlir::dyn_cast<cir::ComplexType>(t)) {
    return mapTypeToC(cplx.getElementType()) + " _Complex";
  }

  // Handle CIR pointer types
  if (auto ptrTy = mlir::dyn_cast<cir::PointerType>(t)) {
    mlir::Type pointee = ptrTy.getPointee();

    // Handle pointer to function
    if (mlir::isa<cir::FuncType>(pointee)) {
      return "void*"; // Simplify function pointers to void*
    }

    // Handle pointer to record (struct/union)
    if (auto recordTy = mlir::dyn_cast<cir::RecordType>(pointee)) {
      mlir::StringAttr nameAttr = recordTy.getName();
      std::string name = (nameAttr && !nameAttr.getValue().empty())
                             ? recordCName(nameAttr)
                             : anonRecordCName(recordTy);
      if (recordTy.isUnion()) {
        return "union " + name + "*";
      }
      return "struct " + name + "*";
    }

    // Handle pointer to vptr (!cir.ptr<!cir.vptr>) -- address of the __vptr
    // slot. The vptr value itself is a void* (vtable pointer), so its address
    // is void**. Mapping this to void* would make subsequent loads through it
    // a dereference of void* (illegal in C).
    if (mlir::isa<cir::VPtrType>(pointee)) {
      return "void**";
    }

    // Handle pointer to array (decays to pointer to element)
    if (auto arrayTy = mlir::dyn_cast<cir::ArrayType>(pointee)) {
      mlir::Type elemType = arrayTy.getElementType();
      std::string elemCType = mapTypeToC(elemType);
      return elemCType + "*";
    }

    // For other pointer types, recursively map the pointee and add *
    std::string pointeeType = mapTypeToC(pointee);
    return pointeeType + "*";
  }

  // Handle CIR vptr type (!cir.vptr) -- vtable pointer value
  if (mlir::isa<cir::VPtrType>(t)) {
    return "void*";
  }

  // Handle CIR record types (struct/union)
  if (auto recordTy = mlir::dyn_cast<cir::RecordType>(t)) {
    mlir::StringAttr nameAttr = recordTy.getName();
    std::string name = (nameAttr && !nameAttr.getValue().empty())
                           ? recordCName(nameAttr)
                           : anonRecordCName(recordTy);
    if (recordTy.isUnion()) {
      return "union " + name;
    }
    return "struct " + name;
  }

  // Handle CIR array types
  if (auto arrayTy = mlir::dyn_cast<cir::ArrayType>(t)) {
    // Arrays in C declarations require the element type, not the full array type
    // This should only be called when we need the base element type
    // Proper array handling should be done at the declaration site where we can
    // properly format "type name[size]"
    mlir::Type elemType = arrayTy.getElementType();
    return mapTypeToC(elemType);
  }

  // Conservative default for unknown types
  return "int";
}

std::string TypeMapper::arrayBaseTypeAndDims(mlir::Type t, std::string &dimsOut) const {
  dimsOut.clear();
  mlir::Type cur = t;
  while (auto arrayTy = mlir::dyn_cast<cir::ArrayType>(cur)) {
    dimsOut += "[" + std::to_string(arrayTy.getSize()) + "]";
    cur = arrayTy.getElementType();
  }
  return mapTypeToC(cur);
}

} // namespace cir2c

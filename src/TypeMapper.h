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

#pragma once

#include <mlir/IR/Types.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <llvm/ADT/DenseMap.h>
#include <map>
#include <set>
#include <string>

namespace cir2c {

/// Service class responsible for mapping CIR/MLIR types to C type strings.
class TypeMapper {
public:
  /// The C struct/union tag of the record named \p nameAttr: the name decided
  /// by planRecordNames, so that different records get different tags.
  std::string recordCName(mlir::StringAttr nameAttr) const;
  /// The same for a record name as text.
  std::string recordCName(llvm::StringRef raw) const;

  /// Decide the C tags of all named records of \p module, before anything
  /// asks for one (issue #8). Sanitizing a C++ name into a C identifier can
  /// give two records one tag, for example `ns::X` and `ns__X`, or two lambdas
  /// at the same location of two files with the same name. A tag decided here
  /// depends only on the set of records, not on the order in which the
  /// emitter meets them.
  void planRecordNames(mlir::ModuleOp module);

  /// A C++ name as a C identifier, without the uniqueness of record tags:
  /// the ".base" suffix and the directories of source locations removed.
  /// Also used for the names of globals, which C keeps apart from tags.
  static std::string plainCName(llvm::StringRef raw);

  /// Map a single CIR/MLIR type to a C type string.
  std::string mapTypeToC(mlir::Type t) const;

  /// Peel off any nested cir::ArrayType layers from `t`, returning the base
  /// (non-array) C type and appending each dimension as "[N]" to `dimsOut`
  /// in outer->inner order.
  std::string arrayBaseTypeAndDims(mlir::Type t, std::string &dimsOut) const;

  /// Stable unique C struct/union tag for an anonymous record type.
  std::string anonRecordCName(mlir::Type recordType) const;

private:
  mutable llvm::DenseMap<mlir::Type, std::string> anonRecordNames_;
  // Record name -> its C tag, and all tags given out. "X.base" has its own tag.
  mutable std::map<std::string, std::string> recordCNames_;
  mutable std::set<std::string> usedRecordCNames_;
};

} // namespace cir2c

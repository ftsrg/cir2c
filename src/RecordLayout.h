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

#include <clang/CIR/Dialect/IR/CIRDialect.h>
#include <clang/CIR/Dialect/IR/CIRTypes.h>
#include <mlir/IR/Types.h>

namespace cir2c {

/// Whether \p memberTy, marked \p kind, occupies no storage as a record member,
/// so the emitted C record must leave it out entirely.
///
/// Two shapes qualify, and both come from upstream making record layout
/// explicit:
///   * a zero-width bit-field, spelled as a zero-length array of the declared
///     type — upstream says it "contributes neither size nor alignment";
///   * a complete record with no members, e.g. `struct {}` — CIR gives it no
///     storage, and any real padding around it is already an explicit `pad`
///     member.
///
/// Emitting either as a C field is not neutral. `int x[0]` carries int's
/// alignment, and an empty record needs a placeholder byte to be valid C at
/// all, so both grow the record they sit in.
///
/// A member left out of the record must also be left out of its brace
/// initializer, which is positional: skip it in one place only and every value
/// after it lands in the wrong field. That is why this lives in one header
/// rather than once per emitter.
inline bool memberOccupiesNoStorage(mlir::Type memberTy,
                                    cir::RecordMemberKind kind) {
  if (cir::isZeroWidthBitField(memberTy, kind))
    return true;
  if (auto rec = mlir::dyn_cast<cir::RecordType>(memberTy))
    return rec.isComplete() && rec.getMembers().empty();
  return false;
}

} // namespace cir2c

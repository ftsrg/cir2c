/*
 * Copyright 2026 Budapest University of Technology and Economics
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

#include "CPrototypes.h"

#include <llvm/Support/JSON.h>
#include <llvm/Support/MemoryBuffer.h>

#include <optional>
#include <regex>
#include <vector>

namespace cir2c {

// clang writes each declaration that matches the filter as a JSON object of
// its own, without a separator. Split the text at the end of each top-level
// object (strings can contain braces).
static std::vector<std::string> splitJsonObjects(const std::string &text) {
  std::vector<std::string> objects;
  int depth = 0;
  bool inString = false, escaped = false;
  size_t start = 0;
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (inString) {
      if (escaped) escaped = false;
      else if (c == '\\') escaped = true;
      else if (c == '"') inString = false;
      continue;
    }
    if (c == '"') inString = true;
    else if (c == '{') {
      if (depth++ == 0) start = i;
    } else if (c == '}' && depth > 0 && --depth == 0) {
      objects.push_back(text.substr(start, i - start + 1));
    }
  }
  return objects;
}

// The pointee const of one parameter type. Yes: a pointer of one level to a
// const-qualified type, for example "const char *", "const void *restrict" or
// "char const *". No: a type without '*' or a pointer to a non-const type.
// Unknown: a type that this reading does not cover, for example a pointer of
// two levels, a function pointer, or an unknown word after the '*'. The
// qualifiers and nullability annotations of the pointer itself (after the '*')
// do not count.
enum class PointeeConst { Yes, No, Unknown };

static PointeeConst pointeeConst(const std::string &qualType) {
  size_t star = qualType.rfind('*');
  if (star == std::string::npos) return PointeeConst::No;
  static const std::regex pointerQualifiers(
      R"(\s*((const|volatile|restrict|__restrict|__restrict__|_Nonnull|_Nullable|_Nullable_result|_Null_unspecified)\s*)*)");
  if (!std::regex_match(qualType.substr(star + 1), pointerQualifiers)) return PointeeConst::Unknown;
  std::string pointee = qualType.substr(0, star);
  if (pointee.find_first_of("*()[") != std::string::npos) return PointeeConst::Unknown;
  static const std::regex constWord(R"((^|\W)const(\W|$))");
  return std::regex_search(pointee, constWord) ? PointeeConst::Yes : PointeeConst::No;
}

bool readCPrototypes(const std::string &path, std::map<std::string, unsigned> &constParams,
                     std::string &err) {
  auto buffer = llvm::MemoryBuffer::getFile(path);
  if (!buffer) {
    err = "cannot read " + path;
    return false;
  }
  for (const std::string &objectText : splitJsonObjects((*buffer)->getBuffer().str())) {
    llvm::Expected<llvm::json::Value> parsed = llvm::json::parse(objectText);
    if (!parsed) {
      err = "invalid JSON in " + path + ": " + llvm::toString(parsed.takeError());
      return false;
    }
    const llvm::json::Object *decl = parsed->getAsObject();
    if (!decl || decl->getString("kind") != "FunctionDecl") continue;
    // The C symbol: the mangled name is the plain name for a function with C
    // linkage, and the asm label of glibc's C++ overloads.
    std::optional<llvm::StringRef> symbol = decl->getString("mangledName");
    if (!symbol) symbol = decl->getString("name");
    if (!symbol || symbol->empty()) continue;
    // A declaration with a parameter type that pointeeConst does not cover
    // gives nothing: then cir2c keeps its table for the function.
    unsigned mask = 0, index = 0;
    bool covered = true;
    if (const llvm::json::Array *inner = decl->getArray("inner")) {
      for (const llvm::json::Value &child : *inner) {
        const llvm::json::Object *node = child.getAsObject();
        if (!node || node->getString("kind") != "ParmVarDecl") continue;
        const llvm::json::Object *type = node->getObject("type");
        std::optional<llvm::StringRef> qualType = type ? type->getString("qualType") : std::nullopt;
        PointeeConst pc = qualType ? pointeeConst(qualType->str()) : PointeeConst::Unknown;
        if (pc == PointeeConst::Unknown || index >= 32) covered = false;
        else if (pc == PointeeConst::Yes) mask |= 1u << index;
        ++index;
      }
    }
    if (covered) constParams[symbol->str()] |= mask;
  }
  return true;
}

} // namespace cir2c

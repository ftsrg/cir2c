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

#pragma once

#include <map>
#include <string>

namespace cir2c {

/// Reads the function declarations that clang wrote with
/// `-Xclang -ast-dump=json -Xclang -ast-dump-filter=<name>` (one or more JSON
/// objects, one after the other) and gives, for each C symbol, the bit mask of
/// the parameters that point to const. Parameter i is in the mask when it is a
/// pointer to a const-qualified type in at least one declaration of the
/// symbol: in C++, glibc declares `memchr`, `strchr`, `strstr`, `wmemchr`, …
/// as two overloads with the C symbol, one with and one without const, and
/// together they give the parameters of the C prototype. Only a pointer of one
/// level counts (`const char *`, not `const char **`): the output writes the
/// const in front of the whole parameter type. A declaration with a parameter
/// type that this reading does not cover (for example `const char **`, or an
/// unknown word after the '*') gives no mask; then cir2c keeps its table of
/// the C library for the function. Returns false and sets `err` when the file
/// cannot be read or parsed.
bool readCPrototypes(const std::string &path, std::map<std::string, unsigned> &constParams,
                     std::string &err);

} // namespace cir2c

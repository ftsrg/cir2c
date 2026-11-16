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

// Issue #2: a handler matches an exception of its own type, of a class that
// has the handler type as an unambiguous public base, and a pointer that
// converts to the handler type ([except.handle]/3). The output selects the
// handler with these rules, and binds the base subobject or the converted
// pointer.

#include <stdexcept>
#include <vector>

struct Base { int b; };
struct Other { int o; };
struct Derived : Base { int d; };
struct Multi : Other, Base { int m; };
struct Left : Base {};
struct Right : Base {};
struct Diamond : Left, Right {};           // two Base subobjects: ambiguous
struct Hidden : private Base {};           // a private base: no match

static int ambiguous() {
  try {
    throw Diamond();
  } catch (Base &) {
    return 100;
  } catch (Diamond &) {
    return 1;
  }
}

static int hidden() {
  try {
    throw Hidden();
  } catch (Base &) {
    return 100;
  } catch (...) {
    return 2;
  }
}

static int rethrown() {
  try {
    try {
      throw Derived{{5}, 6};
    } catch (Base &b) {
      if (b.b != 5) return 100;
      throw;  // the exception keeps its type Derived
    }
  } catch (Derived &d) {
    return d.d == 6 ? 4 : 100;
  }
}

static int byValue() {
  Multi multi;
  multi.o = 1;
  multi.b = 8;
  multi.m = 3;
  try {
    throw multi;
  } catch (Base b) {  // a copy of the Base subobject at offset 4
    return b.b;
  }
}

static int pointers() {
  int r = 0;
  int x = 5;
  try { throw &x; } catch (void *p) { r += (p == &x) ? 16 : 100; }
  Multi *multi = new Multi();
  try {
    throw multi;
  } catch (const Base *p) {  // converted with the offset of Base
    r += (p == static_cast<Base *>(multi)) ? 32 : 100;
  }
  delete multi;
  Multi *none = nullptr;
  try {
    throw none;
  } catch (Base *p) {  // a null pointer stays null
    r += (p == nullptr) ? 64 : 100;
  }
  try { throw nullptr; } catch (int *p) { r += (p == nullptr) ? 128 : 100; }
  return r;
}

static int library() {
  std::vector<int> v(2, 0);
  try {
    v.at(5) = 1;
  } catch (const std::exception &) {  // std::out_of_range
    return 256;
  }
  return 100;
}

int main() {
  int r = 0;
  try { throw Derived(); } catch (Base &) { r += 1; }
  r += ambiguous() + hidden() + rethrown() + byValue() + pointers() + library();
  // 1 + 1 + 2 + 4 + 8 + (16 + 32 + 64 + 128) + 256
  return r == 512 ? 0 : 1;
}

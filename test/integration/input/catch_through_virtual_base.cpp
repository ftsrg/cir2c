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

// A handler that matches the exception only through a virtual base class
// (issue #30). The exception object is a complete object of the thrown class,
// so the virtual base is at a constant offset in it.

#include <cstdio>

struct Base { int id = 42; };
struct Left : virtual Base { int l = 1; };
struct Right : virtual Base { int r = 2; };
struct Diamond : Left, Right { int d = 3; };   // one shared Base part

struct Deep : Diamond { int e = 4; };          // the virtual base further down

int main() {
  int result = 0;
  try {
    throw Diamond();
  } catch (Base &b) {
    printf("Base %d\n", b.id);
    result += b.id == 42 ? 0 : 1;
  }
  try {
    throw Deep();
  } catch (Right &r) {                          // a non-virtual base on the path
    printf("Right %d %d\n", r.r, r.id);
    result += (r.r == 2 && r.id == 42) ? 0 : 2;
  }
  try {
    throw Deep();
  } catch (const Base &b) {
    printf("const Base %d\n", b.id);
    result += b.id == 42 ? 0 : 4;
  }
  return result;
}

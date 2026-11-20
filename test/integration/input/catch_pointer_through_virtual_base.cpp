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

// A pointer exception that a handler matches only through a virtual base class:
// the object can be more derived than the pointer type, so the offset is not
// known during the translation (issue #30).

// cir2c-test-expect-error: through a virtual base class at an offset that is not known

struct Base { int id = 42; };
struct Left : virtual Base {};
struct Right : virtual Base {};
struct Diamond : Left, Right {};

int main() {
  static Diamond d;
  try {
    throw &d;
  } catch (Base *b) {
    return b->id == 42 ? 0 : 1;
  }
  return 2;
}

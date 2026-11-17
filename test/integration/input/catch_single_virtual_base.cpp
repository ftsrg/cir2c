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

// Issue #30: a handler of a virtual base class of a class exception. The
// exception object is a complete Derived, so the virtual base is at a constant
// offset in it. Derived() value-initializes the object, so b.b is 0.

struct Base { int b; };
struct Derived : virtual Base { int d; };

int main() {
  try {
    throw Derived();
  } catch (Base &b) {
    return b.b;
  }
  return 1;
}

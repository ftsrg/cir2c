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

// Issue #2 and #3: a handler of a virtual base class needs the offset of the
// virtual base, which is in the vtable of the object. cir2c does not translate
// virtual bases yet, so it rejects the program. The general rejection of #3
// part 2 comes before the check of the handler selection.

// cir2c-test-expect-error: the program uses a virtual base class

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

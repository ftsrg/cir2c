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

// Issue #3 part 2: with a virtual base, the position of the base subobject is
// in the vtable of the object, and the constructors get the vtables through a
// VTT. cir2c does not translate this yet (the vtables have no virtual-base
// offsets, and the VTT is not real), so it rejects the program instead of
// writing C that gives wrong values.

// cir2c-test-expect-error: the program uses a virtual base class

#include <cassert>

class A {
public:
  A(int x) : m_x(x) {}
  int getX() { return m_x; }
protected:
  int m_x;
};
class B : virtual public A { public: B(int x) : A(x) {} };
class C : virtual public A { public: C(int x) : A(x) {} };
class D : public B, public C { public: D(int x) : A(x), B(x), C(x) {} };

int main() {
  A *a = new D(42);
  assert(a->getX() == 42);
  return 0;
}

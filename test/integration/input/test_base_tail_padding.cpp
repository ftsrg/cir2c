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


// Issue #4: a derived class can place its members in the tail padding of a
// base that is not POD (Itanium C++ ABI). So the base has two layouts: with
// the tail padding as a complete object, without it as a base subobject. The
// output needs both, because the C++ sizes are constants in it: for example,
// the node of a std::set<int> is allocated with its C++ size.

#include <set>

struct Base {
  long a;
  char b;
  Base() : a(1), b(2) {} // not POD, so a derived class can reuse its padding
};

struct Derived : Base {
  char c;
};

int main() {
  Derived d;
  d.c = 3;
  unsigned long offset = (unsigned long)&d.c - (unsigned long)&d;
  if (offset >= sizeof(Base)) return 1;
  // A complete Base keeps its tail padding.
  Base pair[2];
  unsigned long stride = (unsigned long)&pair[1] - (unsigned long)&pair[0];
  if (stride != sizeof(Base)) return 3;
  std::set<int> s;
  for (int i = 0; i < 5; ++i) s.insert(i * 10);
  int sum = 0;
  for (int v : s) sum += v;
  return sum == 100 && d.a == 1 && d.b == 2 && d.c == 3 ? 0 : 2;
}

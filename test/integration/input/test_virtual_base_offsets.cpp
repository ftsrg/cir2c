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

// Issue #3 part 2: a base-class method reads a member of the virtual base, for a standalone
// object and for the same base inside a larger object (different offsets).
#include <cstdio>
struct V { int x; V(int v = 5) : x(v) {} };
struct B : virtual V { int b = 10; int sum() { return x + b; } };
struct M { long pad[3] = {0, 0, 0}; };
struct D : M, B { D() : V(20) {} };
int main() {
  B alone; D big;
  B *pb = &big;
  printf("%d %d %d\n", alone.sum(), big.sum(), pb->sum());
  return 0;
}

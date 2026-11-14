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

// Issue #18: dynamic_cast<void *> reads the offset-to-top entry of the vtable,
// which is not 0 for a secondary base.
#include <cstdio>
struct A { virtual ~A() {} int a = 1; };
struct B { virtual ~B() {} int b = 2; };
struct C : A, B { int c = 3; };
int main() {
  C c;
  B *pb = &c;
  A *pa = &c;
  printf("%d %d\n", dynamic_cast<void *>(pb) == (void *)&c, dynamic_cast<void *>(pa) == (void *)&c);
  return 0;
}

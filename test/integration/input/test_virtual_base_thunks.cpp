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

// Issue #3 part 2: a virtual function of a virtual base, overridden in the most-derived class
// and called through pointers to each subobject (virtual thunks).
#include <cstdio>
struct V { int v = 1; virtual int f() { return v; } virtual ~V() {} };
struct L : virtual V { int l = 2; int fromL() { return v + l; } };
struct R : virtual V { int r = 3; int fromR() { return v + r; } };
struct D : L, R { int d = 4; int f() override { return 100 + v + d; } };
int main() {
  D d;
  V *pv = &d; L *pl = &d; R *pr = &d;
  printf("%d %d %d %d %d\n", pv->f(), pl->f(), pr->f(), pl->fromL(), pr->fromR());
  d.v = 7;
  printf("%d %d %d\n", pv->f(), pl->fromL(), pr->fromR());
  return 0;
}

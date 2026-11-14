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

// Issue #3 part 2: construction with a VTT: the base constructors see their own class, and the
// virtual base is constructed once, first.
#include <cstdio>
static int order[8], n;
struct V { V() { order[n++] = 1; } virtual int id() { return 1; } virtual ~V() {} };
struct L : virtual V { L() { order[n++] = 2 * 10 + id(); } int id() override { return 2; } };
struct R : virtual V { R() { order[n++] = 3 * 10 + id(); } int id() override { return 3; } };
struct D : L, R { D() { order[n++] = 4 * 10 + id(); } int id() override { return 4; } };
int main() {
  { D d; V *v = &d; order[n++] = v->id(); }
  for (int i = 0; i < n; ++i) printf("%d ", order[i]);
  printf("\n");
  return 0;
}

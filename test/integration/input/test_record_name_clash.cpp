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

// Issue #8: different records must get different C tags. Sanitizing a C++
// name into a C identifier can give two records one tag: `ns::X` and `ns__X`,
// and two lambdas at the same line and column of two files with the same
// name. The #line directives below make such files. The two records of each
// pair have different layouts.
namespace ns {
struct X { int a; };
}
struct ns__X { double b[4]; };

template <class F> struct Holder { F f; };

#line 100 "a/util.h"
inline int fa(long   k) { auto g = [k](int x) { return x + (int)k; }; Holder<decltype(g)> h{g}; return h.f(1); }
#line 100 "b/util.h"
inline int fb(double k) { auto g = [k](int x) { return x + (int)k; }; Holder<decltype(g)> h{g}; return h.f(2); }

int main() {
  ns::X p{7};
  ns__X q{{1.0, 2.0, 3.0, 4.0}};
  if (p.a + (int)q.b[3] != 11) return 1;
  return fa(1) + fb(2.5) - 6;
}

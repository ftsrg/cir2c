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
#include <stdio.h>
// An object with a move constructor is passed by reference: the caller sees it moved from.
struct O { int* p; explicit O(int v) : p(new int(v)) {} O(O&& o) noexcept : p(o.p) { o.p = nullptr; } O(const O&) = delete; ~O() { if (p) printf("del %d\n", *p); delete p; } };
struct H { O o; explicit H(O x) : o(static_cast<O&&>(x)) {} };
O mk(int v) { return O(v); }
int take(O x) { return *x.p; }
struct T { int a, b; };
int triv(T t) { t.a = 9; return t.a + t.b; }
int main() { H h(O(1)); printf("%d\n", *h.o.p); O m = mk(2); printf("%d\n", take(static_cast<O&&>(m))); printf("moved %d\n", m.p == nullptr ? 0 : 1); T t{1, 2}; printf("%d %d\n", triv(t), t.a); return 0; }

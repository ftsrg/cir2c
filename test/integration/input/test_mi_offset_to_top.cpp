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

// Issue #3 part 2: multiple inheritance without virtual bases. The secondary vtable has a
// non-zero offset-to-top. The output of the existing code must not change.
#include <cstdio>
struct A { virtual int f() { return 1; } virtual ~A() {} int a = 1; };
struct B { virtual int g() { return 2; } virtual ~B() {} int b = 2; };
struct C : A, B { int f() override { return 10 + a; } int g() override { return 20 + b; } };
int main() { C c; A *pa = &c; B *pb = &c; printf("%d %d\n", pa->f(), pb->g()); return 0; }

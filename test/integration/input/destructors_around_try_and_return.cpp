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
struct T { int id; explicit T(int i) : id(i) { printf("+%d ", id); } T(const T& o) : id(o.id) { printf("c%d ", id); } ~T() { printf("-%d ", id); } };
void th(int x) { T t(100 + x); if (x > 0) throw x; printf("nothrow "); }
int f(int c) { T a(1); { T b(2); if (c == 0) return 10; printf("mid "); } T d(3); if (c == 1) return 20; return 30; }
int g(int c) { T o(4); try { T i(5); th(c); printf("after "); } catch (int e) { printf("caught%d ", e); T h(6); } printf("end "); return c; }
int h(int c) { T o(7); for (int i = 0; i < 3; ++i) { T l(10 + i); if (i == c) break; if (i == 0) continue; printf("loop "); } return 0; }
int main() { for (int i = 0; i < 3; ++i) { f(i); puts(""); } for (int i = 0; i < 2; ++i) { g(i); puts(""); } for (int i = 0; i < 4; ++i) { h(i); puts(""); } try { try { th(1); } catch (char) { puts("no"); } } catch (int) { puts("outer"); } return 0; }

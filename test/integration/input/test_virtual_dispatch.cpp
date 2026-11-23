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

// Each virtual call is dispatched directly to the overriders its static class
// can reach, and an exception object is destroyed by its own destructor.

struct Shape { virtual ~Shape() {} virtual int area() const = 0; virtual int sides() const { return 0; } };
struct Rect : Shape { int w, h; Rect(int a, int b) : w(a), h(b) {} int area() const override { return w * h; } int sides() const override { return 4; } };
struct Square : Rect { Square(int a) : Rect(a, a) {} int sides() const override { return 40; } };
struct Tri : Shape { int b, h; Tri(int x, int y) : b(x), h(y) {} int area() const override { return b * h / 2; } };
struct Other { virtual ~Other() {} virtual int area() const { return -1; } };

static int destroyed = 0;
struct Err { int code; Err(int c) : code(c) {} ~Err() { destroyed += code; } };

int total(const Shape &s) { return s.area() * 100 + s.sides(); }
int rect_only(const Rect &r) { return r.sides(); }

int main() {
  Rect r(2, 3); Square q(5); Tri t(4, 3); Other o;
  if (total(r) != 604) return 1;
  if (total(q) != 2540) return 2;
  if (total(t) != 600) return 3;
  if (rect_only(q) != 40 || rect_only(r) != 4) return 4;
  Shape *shapes[] = { &r, &q, &t };
  int sum = 0;
  for (int i = 0; i < 3; ++i) sum += shapes[i]->area();
  if (sum != 37) return 5;
  if (o.area() != -1) return 6;
  try { throw Err(7); } catch (const Err &e) { if (e.code != 7) return 7; }
  if (destroyed != 7) return 8;
  return 0;
}

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

// Issue #11: the calling convention passes a small struct as one or two
// register values. When each register value is exactly one scalar member of
// the struct (same offset, same size, same kind), the output converts the
// members one by one, and needs no union: `f((void*)it.p)`. The structs of
// test_call_conv_union.cpp pack several members into one value, so they keep
// the union.
//
// cir2c-test-output-lacks: union {
// cir2c-test-log-lacks: pointer cast to another type

struct Iter { int *p; };                             // one pointer
struct Span { const char *p; unsigned long n; };     // pointer and integer
struct D16 { double x, y; };                         // two doubles
struct M16 { double x; long n; };                    // double and integer
struct Wrapped { Iter it; long n; };                 // nested struct
struct Node { Node *next; Node *prev; };             // pointers to its own type
struct L2 { long a; long b; };                       // two integers

static int values[4] = {1, 2, 3, 4};

Iter advance(Iter it) { return Iter{it.p + 1}; }
Span shorten(Span s) { return Span{s.p, s.n - 1}; }
D16 swap(D16 d) { return D16{d.y, d.x}; }
M16 twice(M16 m) { return M16{2 * m.x, 2 * m.n}; }
Wrapped step(Wrapped w) { return Wrapped{advance(w.it), w.n + 1}; }
Node relink(Node n) { return Node{n.prev, n.next}; }
L2 sum(L2 a, L2 b) { return L2{a.a + b.a, a.b + b.b}; }

struct Shape {
  virtual D16 scale(D16 d) const { return d; }
  virtual ~Shape() {}
};
struct Square : Shape {
  D16 scale(D16 d) const override { return D16{3 * d.x, 3 * d.y}; }
};

int main() {
  Iter (*through)(Iter) = advance;
  Iter it = through(advance(Iter{values}));
  Span s = shorten(Span{"abcdef", 6});
  D16 d = swap(D16{1.5, 2.5});
  M16 m = twice(M16{0.25, 21});
  Wrapped w = step(Wrapped{Iter{values}, 7});
  Node a{nullptr, nullptr}, b{&a, nullptr};
  Node r = relink(b);
  L2 l = sum(L2{1, 2}, L2{30, 40});
  Square sq;
  const Shape &shape = sq;
  D16 scaled = shape.scale(D16{1, 2});
  bool ok = *it.p == 3 && s.n == 5 && s.p[0] == 'a' && d.x == 2.5 && d.y == 1.5 &&
            m.x == 0.5 && m.n == 42 && *w.it.p == 2 && w.n == 8 && r.next == nullptr &&
            r.prev == &a && l.a == 31 && l.b == 42 && scaled.x == 3 && scaled.y == 6;
  return ok ? 0 : 1;
}

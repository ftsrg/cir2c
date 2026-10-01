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


// Issue #11: the calling convention of the target is already in the CIR. A
// small struct passed or returned by value travels as one or two integer,
// pointer or floating-point values, and the CIR converts between the struct and
// these values through a local variable that it reads as the other type. In C
// such a read is undefined (C11 6.5p7), so the variable must be a union. The
// padding of a struct must not reach an integer uninitialized.
//
// cir2c-test-log-lacks: pointer cast to another type

struct I4 { int a; };
struct I8 { char c; int i; };                  // 3 bytes of padding
struct I12 { int a, b, c; };                   // travels as 8 + 4 bytes
struct I16 { const char *p; unsigned long n; };
struct I24 { long a, b, c; };                  // too large: in memory
struct D16 { double x, y; };
struct M16 { double x; long n; };
struct N8 { struct { char c; short s; } in; char d; };  // nested padding
struct A8 { char b[3]; int i; };               // array member, padding
struct B8 { bool f; int i; };

template <class T> T id(T v) { return v; }

struct Base {
  virtual I8 get(I8 v) const { return v; }
  virtual D16 swap(D16 v) const { return D16{v.y, v.x}; }
  virtual ~Base() {}
};
struct Derived : Base {
  I8 get(I8 v) const override { v.i += 1; return v; }
};

static const char text[] = "abc";

int main() {
  int bad = 0;
  I4 a4 = id(I4{4});
  if (a4.a != 4) bad |= 1 << 0;
  I8 a8 = id(I8{'x', 8});
  if (a8.c != 'x' || a8.i != 8) bad |= 1 << 1;
  I12 a12 = id(I12{1, 2, 3});
  if (a12.a + a12.b + a12.c != 6) bad |= 1 << 2;
  I16 a16 = id(I16{text, 3});
  if (a16.p[2] != 'c' || a16.n != 3) bad |= 1 << 3;
  I24 a24 = id(I24{1, 2, 3});
  if (a24.c != 3) bad |= 1 << 4;
  D16 d16 = id(D16{1.5, 2.5});
  if (d16.x + d16.y != 4.0) bad |= 1 << 5;
  M16 m16 = id(M16{2.0, 7});
  if (m16.x != 2.0 || m16.n != 7) bad |= 1 << 7;
  N8 n8 = id(N8{{'n', 300}, 'd'});
  if (n8.in.c != 'n' || n8.in.s != 300 || n8.d != 'd') bad |= 1 << 8;
  A8 b8 = id(A8{{'p', 'q', 'r'}, 9});
  if (b8.b[2] != 'r' || b8.i != 9) bad |= 1 << 9;
  B8 t8 = id(B8{true, 10});
  if (!t8.f || t8.i != 10) bad |= 1 << 10;

  I8 (*through)(I8) = id<I8>;
  I8 p8 = through(I8{'y', 11});
  if (p8.c != 'y' || p8.i != 11) bad |= 1 << 11;

  Base *object = new Derived;
  I8 v8 = object->get(I8{'z', 12});
  if (v8.c != 'z' || v8.i != 13) bad |= 1 << 12;
  D16 w16 = object->swap(D16{1.0, 2.0});
  if (w16.x != 2.0 || w16.y != 1.0) bad |= 1 << 13;
  delete object;
  return bad;
}

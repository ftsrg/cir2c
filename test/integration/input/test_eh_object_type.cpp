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

// Issue #12: each throw site keeps the exception object in a static variable of
// the thrown type. A static char buffer, cast to the thrown type, is not
// correct C: the stores through the cast pointer access a char array as another
// type (C11 6.5p7), and the array has no alignment for the thrown type. clang
// -Wcast-align cannot examine this, because the output also casts char
// pointers for the base classes.
//
// cir2c-test-output-lacks: static char exc_buf

struct Wide {
  double d;
  long n;
  virtual ~Wide() {}
  Wide(double d, long n) : d(d), n(n) {}
};

struct Point { int x, y; };

int depth;

long throwWide(int x) {
  if (x > 2) throw Wide(0.5 * x, x);
  return x;
}

int throwPoint(int x) {
  if (x > 2) throw Point{x, -x};
  return x;
}

int rethrowWide(int x) {
  try {
    return (int)throwWide(x);
  } catch (const Wide &w) {
    depth = 1;
    throw;
  }
}

int main() {
  int result = 0;
  try {
    throwWide(5);
    result |= 1;
  } catch (const Wide &w) {
    if (w.d != 2.5 || w.n != 5) result |= 2;
  }
  try {
    throwPoint(7);
    result |= 4;
  } catch (Point p) {
    if (p.x != 7 || p.y != -7) result |= 8;
  }
  try {
    rethrowWide(3);
    result |= 16;
  } catch (const Wide &w) {
    if (depth != 1 || w.d != 1.5 || w.n != 3) result |= 32;
  }
  return result;
}

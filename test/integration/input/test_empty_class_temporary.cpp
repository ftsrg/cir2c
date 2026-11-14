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

// Issue #12: an empty class has no data for argument passing, so the calling
// convention removes it from the arguments of a call. The CIR still loads the
// temporary of the argument, and nothing writes that temporary before. In C,
// the copy of an uninitialized local whose address is not taken is undefined
// (C11 6.3.2.1p2). clang reports it with -Wuninitialized.
//
// cir2c-test-cflags: -Werror=uninitialized

struct Less {
  bool operator()(int a, int b) const { return a < b; }
};

struct Tag {};

struct Holder {
  Less cmp;
  int x;
  Holder(Less c, int x) : cmp(c), x(x) {}
};

int pick(Less c, int a, int b) { return c(a, b) ? a : b; }

int tagged(Tag, int v, Tag) { return v + 1; }

// A record whose only member is an empty class has no data either, for example
// a lambda that captures only an empty function object (as libc++'s radix sort
// does). Nothing writes the lambda object, and the call copies it.
struct Identity {
  int operator()(int x) const { return x; }
};

template <class F> int apply(F f, int v) { return f(v); }

int mapped(Identity id, int v) {
  auto plusOne = [id = id](int x) { return id(x) + 1; };
  return apply(plusOne, v);
}

int main() {
  Holder h(Less(), 3);
  int r = pick(Less(), h.x, 5);
  int s = tagged(Tag(), r, Tag());
  int w = mapped(Identity(), s);
  return (r == 3 && s == 4 && w == 5 && h.cmp(1, 2)) ? 0 : 1;
}

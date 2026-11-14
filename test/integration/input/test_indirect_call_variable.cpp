/*
 * Copyright 2026 Budapest University of Technology and Economics
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

// Each call through a function pointer goes through a variable of the
// function-pointer type, not through a cast expression: an indirect call of
// the program, a virtual call, and the destructor call of a caught exception.
// Theta rejects a call whose callee is a cast expression.
// cir2c-test-output-contains: int (*fp
// cir2c-test-output-contains: = (int (*)(void*))__fn;
// cir2c-test-output-contains: void (*__dtor)(void*) = (void (*)(void*))__cir_exc_dtor;

static int twice(int x) { return 2 * x; }
static int apply(int (*f)(int), int x) { return f(x); }

struct Shape {
  virtual int sides() const { return 0; }
  virtual ~Shape() {}
};
struct Square : Shape {
  int sides() const override { return 4; }
};

static int destroyed = 0;
struct Error {
  int code;
  explicit Error(int c) : code(c) {}
  ~Error() { ++destroyed; }
};

static int count(const Shape &s) { return s.sides(); }

int main() {
  int result = apply(&twice, 3);
  Square square;
  result += count(square);
  try {
    throw Error(5);
  } catch (const Error &e) {
    result += e.code;
  }
  return result + destroyed;  // 6 + 4 + 5 + 1
}

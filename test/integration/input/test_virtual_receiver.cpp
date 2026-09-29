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

// Issue #5: a call through a vtable, and the destruction of a caught
// exception, call the function through a pointer with a void* receiver. The
// function types must agree, also for a covariant override. The runner cannot
// see a disagreement: it shows only under -fsanitize=function.
struct Base {
  virtual ~Base() {}
  virtual Base *self() { return this; }
  virtual int value(int k) const { return k; }
};

struct Derived : Base {
  int extra = 5;
  Derived *self() override { return this; }
  int value(int k) const override { return k + extra; }
};

struct Thrown {
  int *counter;
  ~Thrown() { ++*counter; }
};

int main() {
  Derived d;
  Base *b = &d;
  if (b->self() != b) return 1;
  if (b->value(2) != 7) return 2;
  Base *h = new Derived;
  delete h;
  int destroyed = 0;
  try {
    throw Thrown{&destroyed};
  } catch (Thrown &) {
  }
  if (destroyed != 1) return 3;
  return 0;
}

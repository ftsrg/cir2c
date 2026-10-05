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

// Issue #12 (#9 part 1): a destructor is noexcept unless it says otherwise. An
// exception that leaves it calls std::terminate, which ends the program with
// SIGABRT. The handler in main() does not run.

struct Positive {
  int value;
};

static int counter = 0;

struct Thrower {
  int value;
  ~Thrower() {
    if (value > 0) throw Positive{value};
    counter = 1;
  }
};

static int scope(int x) {
  {
    Thrower t{x};
  }
  return counter + 4;
}

int main() {
  try {
    return scope(1);
  } catch (Positive) {
    return 3;
  }
}

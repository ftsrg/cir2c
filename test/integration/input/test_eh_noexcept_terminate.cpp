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

// Issue #12 (#9 part 1): an exception that leaves a noexcept function calls
// std::terminate ([except.spec]p5), which ends the program with SIGABRT. The
// handler in main() does not run, and wrapper() does not continue.

struct Positive {
  int value;
};

static int counter = 0;

static void inner(int x) {
  if (x > 0) throw Positive{x};
}

static void wrapper(int x) noexcept {
  inner(x);
  counter = 1;
}

int main() {
  try {
    wrapper(1);
  } catch (Positive) {
    return 3;
  }
  return counter + 4;
}

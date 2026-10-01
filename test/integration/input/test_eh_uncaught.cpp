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

// Issue #6: an exception that leaves main() calls std::terminate, which ends
// the program with SIGABRT. It does not return from main().
// A type of the program: the test does not depend on a library model.
struct Positive {
  int value;
};

static int thrower(int x) {
  if (x > 0) throw Positive{x};
  return x;
}

int main() { return thrower(1); }

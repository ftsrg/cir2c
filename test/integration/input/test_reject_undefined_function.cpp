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


// With --no-externalize-std, cir2c rejects a program whose output uses a
// function without a definition outside the C standard library (README 8.2).
// Here a C++ function and a C function have no definition.

// cir2c-test-expect-error: These have no definition:
// cir2c-test-expect-error:   helper(int)
// cir2c-test-expect-error:   nondet_int

#include <cstdlib>

extern "C" int nondet_int(void);
int helper(int value);

int main() {
  int value = helper(nondet_int());
  return std::abs(value) > 10 ? 1 : 0;
}

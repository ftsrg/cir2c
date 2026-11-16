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

// CBMC's goto-cc stops with the invariant "expected string constant" when the
// text arguments of __assert_fail are variables. The arguments of assert() are
// literals, so the output must pass them as literals too.

// cir2c-test-output-lacks: __assert_fail(cast
// cir2c-test-output-contains: __assert_fail("

#include <cassert>

int main(int argc, char **) {
  assert(argc >= 1 && "at least the program name");
  return 0;
}

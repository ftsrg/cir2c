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

// Issue #12: the message of an assertion contains __FILE__, and the runner
// gives the path of this file with its directory. The output must not contain
// the local directory of the translation.
//
// cir2c-test-output-lacks: integration/input/

#include <assert.h>

int twice(int x) { return 2 * x; }

int main(void) {
  int v = twice(21);
  assert(v == 42);
  return v == 42 ? 0 : 1;
}

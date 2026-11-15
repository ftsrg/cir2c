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

// README 8.1: a vector member has no C type. Before, the member became an int,
// so the struct lost 12 bytes and a copy of sizeof(S) bytes overflowed.

// cir2c-test-expect-error: the output uses a type that C has no type for
// cir2c-test-expect-error: !cir.vector<4 x !cir.int<s, 32>>

#include <cstring>

typedef int v4si __attribute__((vector_size(16)));
struct S { v4si v; int x; };

int main() {
  S a, b;
  a.x = 5;
  std::memcpy(&b, &a, sizeof a);
  return b.x - 5;
}

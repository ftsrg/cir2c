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

// Issue #3 part 2: a member of a virtual base, read from local and global
// complete objects. Their vtable entries are constants.
#include <cstdio>
struct V { int x = 3; };
struct B : virtual V { int b = 4; };
struct D : B { int d = 5; };
D global;
int main() {
  B local;
  D derived;
  printf("%d %d %d\n", local.x, derived.x, global.x);
  return 0;
}

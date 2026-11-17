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

// An empty virtual base class that two paths of the thrown class reach (issue
// #30, like the ESBMC task nec_ex4-diamond-shared-inheritance). The record of
// the complete object has no member for an empty base, so cir2c reads its
// offset from the primary vtable, through the path at offset 0, and uses it for
// the other path too.

#include <cstdio>

struct Stream {};
struct Input : virtual Stream {};
struct Output : virtual Stream {};
struct InOut : Input, Output {};

int main() {
  int caught = 0;
  try {
    throw InOut();
  } catch (InOut &) {
    printf("InOut\n");
    caught += 1;
  }
  try {
    throw InOut();
  } catch (Stream &) {
    printf("Stream\n");
    caught += 2;
  }
  try {
    throw Output();
  } catch (Stream &) {
    printf("Stream from Output\n");
    caught += 4;
  }
  return caught == 7 ? 0 : 1;
}

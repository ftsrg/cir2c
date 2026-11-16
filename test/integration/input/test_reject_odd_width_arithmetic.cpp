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

// README 8.1: an integer of 24 bits travels in an int. That keeps the bits of a
// value that is only stored or passed, but the arithmetic would not wrap at 24
// bits: x++ must give 0 here, the int gives 0x1000000.

// cir2c-test-expect-error: computes with a 24-bit integer, which C has no type for

int main() {
  unsigned _BitInt(24) x = 0xFFFFFF;
  x++;
  return x == 0 ? 0 : 1;
}

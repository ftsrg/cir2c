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

// README 8.1: an integer wider than 64 bits has no C type. Before, it became an
// int, and the shift by 70 bits was undefined.

// cir2c-test-expect-error: computes with a 100-bit integer, which C has no type for

int main() {
  unsigned _BitInt(100) b = 1;
  b <<= 70;
  return (b >> 70) == 1 ? 0 : 1;
}

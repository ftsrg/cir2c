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

// clang writes a long double constant as the shortest decimal for the 80-bit
// format, and the CIR parser reads a decimal as a double. cir2c gives the
// parser the bit pattern instead, so the constants keep their 64-bit mantissa
// and the values outside the range of double.

#include <float.h>
#include <stdio.h>

long double tenth = 0.1L;
long double third = 1.0L / 3.0L;
long double tiny = LDBL_TRUE_MIN;
long double huge = LDBL_MAX;
long double big = 1e310L;

int main(void) {
  volatile long double x = 0.1L;
  printf("%.21Lg %.21Lg %.21Lg %.21Lg %.21Lg\n", tenth, third, tiny, huge, big);
  printf("%d %d %d\n", x == tenth, tiny > 0, huge * 2 > huge);
  return 0;
}

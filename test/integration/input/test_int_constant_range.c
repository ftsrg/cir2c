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


// Issue #12: an integer constant whose signed reading is not its value. The
// unsigned constant 1 << 63 was written as -9223372036854775808, which is not
// a C constant (9223372036854775808 fits in no signed type, C11 6.4.4p2), and
// LONG_MIN the same way, also in globals and case labels. gcc and clang
// warn, and -Werror stops the build.
//
// cir2c-test-cflags: -Werror=implicitly-unsigned-literal

#include <limits.h>
#include <stdio.h>

static const unsigned long sign_mask_global = 1UL << 63;
static const long long_min_global = LONG_MIN;
static const unsigned int u32_max_global = 0xFFFFFFFFu;

static int classify(unsigned long x, long y) {
  int r = 0;
  switch (x) {
  case 1UL << 63: r = 1; break;
  case 0xFFFFFFFFFFFFFFFFUL: r = 2; break;
  default: r = 3; break;
  }
  switch (y) {
  case LONG_MIN: r += 10; break;
  default: r += 20; break;
  }
  return r;
}

int main(void) {
  unsigned long sign_mask = 1UL << 63;
  long long_min = LONG_MIN;
  unsigned int u32_max = 0xFFFFFFFFu;
  unsigned char u8_max = 0xFF;
  unsigned long long u64_max = 0xFFFFFFFFFFFFFFFFULL;
  int bad = 0;
  if (sign_mask != sign_mask_global || (sign_mask >> 63) != 1) bad |= 1;
  if (long_min != long_min_global || long_min + 1 != -LONG_MAX) bad |= 2;
  if (u32_max != u32_max_global || u32_max + 1u != 0) bad |= 4;
  if (u8_max != 255) bad |= 8;
  if (u64_max + 1 != 0) bad |= 16;
  if (classify(sign_mask, long_min) != 11 || classify(u64_max, 0) != 22) bad |= 32;
  printf("%lu %ld %u %u %llu\n", sign_mask, long_min, u32_max, (unsigned)u8_max, u64_max);
  return bad;
}

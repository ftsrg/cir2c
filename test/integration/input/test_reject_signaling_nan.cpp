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

// README 8.1: the output tests the classes of a floating-point value with
// comparisons, and a comparison cannot tell a quiet NaN from a signaling one.
// __builtin_issignaling would then be true for each NaN, so cir2c stops.

// cir2c-test-expect-error: tells a quiet NaN from a signaling NaN

#include <cmath>

int main(int argc, char **) {
  double x = argc > 5 ? 1.0 : NAN;
  return __builtin_issignaling(x) ? 1 : 0;
}

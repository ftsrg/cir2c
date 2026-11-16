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

// Floating-point classification (README 8.1): each class of cir.is_fp_class
// for float, double and long double, at the limits of each class.

#include <float.h>
#include <math.h>
#include <stdio.h>

static volatile float values_float[] = { (float)(0.0f), (float)(-0.0f), (float)(FLT_TRUE_MIN), (float)(-(FLT_MIN - FLT_TRUE_MIN)), (float)(FLT_MIN), (float)(-1.0f), (float)(FLT_MAX), (float)(INFINITY), (float)(-INFINITY), (float)(NAN) };
static void classify_float(void) {
  for (unsigned i = 0; i < sizeof values_float / sizeof values_float[0]; ++i) {
    float x = values_float[i];
    printf("float %u:", i);
    printf(" %d", __builtin_isfpclass(x, 3));
    printf(" %d", __builtin_isfpclass(x, 4));
    printf(" %d", __builtin_isfpclass(x, 8));
    printf(" %d", __builtin_isfpclass(x, 16));
    printf(" %d", __builtin_isfpclass(x, 32));
    printf(" %d", __builtin_isfpclass(x, 64));
    printf(" %d", __builtin_isfpclass(x, 128));
    printf(" %d", __builtin_isfpclass(x, 256));
    printf(" %d", __builtin_isfpclass(x, 512));
    printf(" %d", __builtin_isfpclass(x, 516));
    printf(" %d", __builtin_isfpclass(x, 504));
    printf(" %d", __builtin_isfpclass(x, 264));
    printf(" %d", __builtin_isfpclass(x, 96));
    printf(" %d", __builtin_isfpclass(x, 144));
    printf(" %d", __builtin_isfpclass(x, 1020));
    printf(" %d", __builtin_isfpclass(x, 1023));
    printf(" | %d %d %d %d %d %d\n", !!isnan(x), !!isinf(x), !!isfinite(x), !!isnormal(x), fpclassify(x), !!signbit(x));
  }
}
static volatile double values_double[] = { (double)(0.0), (double)(-0.0), (double)(DBL_TRUE_MIN), (double)(-(DBL_MIN - DBL_TRUE_MIN)), (double)(DBL_MIN), (double)(-1.0), (double)(DBL_MAX), (double)(INFINITY), (double)(-INFINITY), (double)(NAN) };
static void classify_double(void) {
  for (unsigned i = 0; i < sizeof values_double / sizeof values_double[0]; ++i) {
    double x = values_double[i];
    printf("double %u:", i);
    printf(" %d", __builtin_isfpclass(x, 3));
    printf(" %d", __builtin_isfpclass(x, 4));
    printf(" %d", __builtin_isfpclass(x, 8));
    printf(" %d", __builtin_isfpclass(x, 16));
    printf(" %d", __builtin_isfpclass(x, 32));
    printf(" %d", __builtin_isfpclass(x, 64));
    printf(" %d", __builtin_isfpclass(x, 128));
    printf(" %d", __builtin_isfpclass(x, 256));
    printf(" %d", __builtin_isfpclass(x, 512));
    printf(" %d", __builtin_isfpclass(x, 516));
    printf(" %d", __builtin_isfpclass(x, 504));
    printf(" %d", __builtin_isfpclass(x, 264));
    printf(" %d", __builtin_isfpclass(x, 96));
    printf(" %d", __builtin_isfpclass(x, 144));
    printf(" %d", __builtin_isfpclass(x, 1020));
    printf(" %d", __builtin_isfpclass(x, 1023));
    printf(" | %d %d %d %d %d %d\n", !!isnan(x), !!isinf(x), !!isfinite(x), !!isnormal(x), fpclassify(x), !!signbit(x));
  }
}
static volatile long double values_long_double[] = { (long double)(0.0L), (long double)(-0.0L), (long double)(LDBL_TRUE_MIN), (long double)(-(LDBL_MIN - LDBL_TRUE_MIN)), (long double)(LDBL_MIN), (long double)(-1.0L), (long double)(LDBL_MAX), (long double)(INFINITY), (long double)(-INFINITY), (long double)(NAN) };
static void classify_long_double(void) {
  for (unsigned i = 0; i < sizeof values_long_double / sizeof values_long_double[0]; ++i) {
    long double x = values_long_double[i];
    printf("long_double %u:", i);
    printf(" %d", __builtin_isfpclass(x, 3));
    printf(" %d", __builtin_isfpclass(x, 4));
    printf(" %d", __builtin_isfpclass(x, 8));
    printf(" %d", __builtin_isfpclass(x, 16));
    printf(" %d", __builtin_isfpclass(x, 32));
    printf(" %d", __builtin_isfpclass(x, 64));
    printf(" %d", __builtin_isfpclass(x, 128));
    printf(" %d", __builtin_isfpclass(x, 256));
    printf(" %d", __builtin_isfpclass(x, 512));
    printf(" %d", __builtin_isfpclass(x, 516));
    printf(" %d", __builtin_isfpclass(x, 504));
    printf(" %d", __builtin_isfpclass(x, 264));
    printf(" %d", __builtin_isfpclass(x, 96));
    printf(" %d", __builtin_isfpclass(x, 144));
    printf(" %d", __builtin_isfpclass(x, 1020));
    printf(" %d", __builtin_isfpclass(x, 1023));
    printf(" | %d %d %d %d %d %d\n", !!isnan(x), !!isinf(x), !!isfinite(x), !!isnormal(x), fpclassify(x), !!signbit(x));
  }
}
int main(void) {
  classify_float();
  classify_double();
  classify_long_double();
  return 0;
}

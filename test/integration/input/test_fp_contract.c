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

// Integration test: FP contraction (cir.fmuladd).
//
// Under the default -ffp-contract=on, clang emits a single cir.fmuladd for
// `a * b + c` instead of cir.fmul + cir.fadd. cir2c must map it; before it did
// the mapper hard-failed with "No handler registered for op: cir.fmuladd" on
// any ordinary floating-point expression of this shape.
//
// Self-checking: returns 0 only if every contracted expression is correct.

int test_float_contract(void) {
    float a = 2.0f, b = 3.0f, c = 1.0f;
    return (a * b + c) == 7.0f;          // cir.fmuladd
}

int test_double_contract(void) {
    double a = 2.0, b = 3.0, c = 10.0;
    return (c - a * b) == 4.0;           // cir.fmuladd with a negated operand
}

int test_contract_in_loop(void) {
    double acc = 0.0;
    for (int i = 1; i <= 4; i++)
        acc = acc * 2.0 + (double)i;     // cir.fmuladd, loop-carried
    return acc == 26.0;                  // ((0*2+1)*2+2)*2+3)*2+4
}

int main(void) {
    if (!test_float_contract())  return 1;
    if (!test_double_contract()) return 2;
    if (!test_contract_in_loop()) return 3;
    return 0;
}

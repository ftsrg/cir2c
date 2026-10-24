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

/* This is the function for which it is desired to find more efficient
code.  It must have either one or two arguments, both int, and must
return a 32-bit int quantity.  It is declared in aha.h. */

int userfun(int x) {
// if (x > 0) return 1;                 // x > 0 predicate.
// else return 0;                       // Turn off div & divu.
                                        // Found a new formula for HD.

// if (x >= 32) return 0;
// return 1 << (unsigned)x;

// return 3*x + 1;

//   if (x >= 0) return x;                // Absolute value.
//   else return -x;

 return (x & 0xfffffffc) | ((x & 1) << 1) | ((x & 2) >> 1);
                                        // Swap rightmost 2 bits.
}

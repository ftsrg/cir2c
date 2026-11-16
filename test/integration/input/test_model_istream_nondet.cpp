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

// The istream-nondet model (README 8.2): an extraction from std::cin gives an
// unknown value, from __VERIFIER_nondet_<type>. This test defines these
// functions, so that the output links and runs. The result does not depend on
// the values: the native run reads its own input.

#include <cassert>
#include <iostream>

extern "C" int __VERIFIER_nondet_int(void) { return 21; }
extern "C" double __VERIFIER_nondet_double(void) { return 0.5; }
extern "C" unsigned long __VERIFIER_nondet_ulong(void) { return 7; }

int main() {
  int a = 1;
  double d = 0;
  unsigned long u = 0;
  std::cin >> a >> d >> std::ws >> u;
  std::cin.ignore();
  long long twice = 2LL * a;
  assert(twice % 2 == 0);
  std::cout << "values read" << std::endl;
  return (d == d || d != d) ? 0 : 1;
}

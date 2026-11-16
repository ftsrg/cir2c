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

// In the istream-nondet model (README 8.2), an extraction never fails. A loop
// like `while (cin >> x)` would not end, but in the program it ends at the end
// of the input. Thus the model has no stream state for input.

// cir2c-test-expect-error: These have no definition:
// cir2c-test-expect-error: basic_istream<char, std::__1::char_traits<char> >::operator bool() const

#include <iostream>

int main() {
  int value = 0;
  int sum = 0;
  while (std::cin >> value)
    sum += value;
  return sum > 100 ? 1 : 0;
}

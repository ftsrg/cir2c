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

// The istream-nondet model (README 8.2) has no input of characters and
// strings. A model of such input needs a bound on the length.

// cir2c-test-expect-error: These have no definition:
// cir2c-test-expect-error: getline
// cir2c-test-expect-error: char&

#include <iostream>
#include <string>

int main() {
  std::string line;
  std::getline(std::cin, line);
  char c = 'a';
  std::cin >> c;
  return line.size() > 3 && c == 'x' ? 1 : 0;
}

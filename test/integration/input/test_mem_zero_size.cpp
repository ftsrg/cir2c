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

// Issue #12: memcpy, memmove and the other functions of this group must not
// get a null pointer, also with a size of zero (C11 7.24.1p2). libc++ calls
// them for an empty range, for example memmove from the null data of an empty
// vector. The output makes no call when the size is zero.
//
// cir2c-test-output-lacks: = memcpy(
// cir2c-test-output-lacks: = memmove(

#include <string>
#include <vector>

int main() {
  std::vector<int> empty;
  std::vector<int> copy(empty);
  std::vector<int> values(3, 7);
  std::vector<int> target;
  target = empty;
  target.insert(target.end(), values.begin(), values.end());
  std::string a;
  std::string b(a);
  b += "xy";
  return (copy.empty() && target.size() == 3 && target[2] == 7 && b == "xy") ? 0 : 1;
}

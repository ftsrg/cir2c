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

// Issue #5: C++ defines a null pointer plus 0 and null minus null; C does not.
// libc++ does both with an empty vector. The runner cannot see the undefined
// behavior. -fsanitize=pointer-overflow shows the first one; no sanitizer
// shows the second one.
#include <vector>

int main() {
  std::vector<int> v;
  int *end = v.data() + v.size();
  if (end != v.data()) return 1;
  if (v.end() - v.begin() != 0) return 2;
  std::vector<int> w(v.begin(), v.end());
  return w.empty() ? 0 : 3;
}

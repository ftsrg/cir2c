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


// The libcxx-out-of-line model (README 8.2). libc++'s own header code throws
// length_error, out_of_range and bad_array_new_length, and uses std::nothrow.
// libc++ keeps some of their members, and std::nothrow, in its compiled
// library. The native reference of this test suite uses libstdc++, so the test
// compares only the what() texts that both libraries share.

#include <cstddef>
#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>

int main() {
  // Each catch names the exact type, because cir2c matches only the exact
  // type (README 8.3).
  std::vector<int> v(3);
  try {
    v.at(20);
    return 1;
  } catch (std::out_of_range &e) {
    if (e.what()[0] == '\0') return 2;
  }
  try {
    std::vector<int> huge(static_cast<std::size_t>(-1)); // more than max_size()
    return 3;
  } catch (std::length_error &e) {
    if (e.what()[0] == '\0') return 4;
  }
  std::allocator<int> alloc;
  try {
    (void)alloc.allocate(static_cast<std::size_t>(-1));
    return 5;
  } catch (std::bad_array_new_length &e) {
    if (e.what()[0] == '\0') return 6;
  }
  std::exception plain;
  if (std::strcmp(plain.what(), "std::exception") != 0) return 7;
  std::bad_alloc noMemory;
  if (std::strcmp(noMemory.what(), "std::bad_alloc") != 0) return 8;
  int *number = new (std::nothrow) int(5);
  if (!number || *number != 5) return 9;
  delete number;
  return 0;
}

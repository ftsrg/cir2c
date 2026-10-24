/*
 * Copyright 2026 Budapest University of Technology and Economics
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

// A C library function that the program declares but does not define gets
// the const of its standard prototype from cir2c's table: the CIR has no const
// (llvm/llvm-project#217687). __assert_fail of glibc is in the table too.
// cir2c-test-output-contains: extern void __assert_fail(const char* p0, const char* p1, unsigned int p2, const char* p3);
// cir2c-test-output-contains: extern unsigned long strlen(const char* p0);

#include <cassert>
#include <cstring>

int main() {
  const char *word = "abc";
  assert(std::strlen(word) == 3);
  return static_cast<int>(std::strlen(word));
}

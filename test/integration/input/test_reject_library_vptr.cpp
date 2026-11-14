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

// Issue #3 part 2: with --externalize-std, the constructor of a library object
// is unknown, so its vtable pointer is unknown too. The conversion to the
// virtual base std::basic_ios reads that pointer, and cir2c stops. Without
// --externalize-std, the ostream-sink model has no <fstream>, and clang stops.

// cir2c-test-expect-error-externalize-std: is unknown, so its vtable entries cannot be read

#include <fstream>

int main() {
  std::ofstream out("/dev/null");
  if (!out) return 1;
  return 0;
}

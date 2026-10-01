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


// The ostream-sink model (README 8.2) does not model string streams. Its
// <sstream> stops the compilation with a clear message, not with an error
// inside the libc++ stream code that the model replaces.

// cir2c-test-expect-error: cir2c model ostream-sink: <sstream> is not modeled

#include <sstream>

int main() {
  std::ostringstream out;
  out << 42;
  return out.str() == "42" ? 0 : 1;
}

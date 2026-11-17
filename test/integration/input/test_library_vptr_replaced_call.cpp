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

// Issue #32: `!out` converts the stream to its virtual base std::basic_ios,
// which reads the vptr of `out`. The vtable of std::ofstream is in the compiled
// libc++, so no code of the output sets that vptr. With --externalize-std, the
// call of operator! is replaced, so the conversion has no use and emits
// nothing. Without --externalize-std, the ostream-sink model has no <fstream>,
// and clang stops.

#include <fstream>

int main() {
  std::ofstream out("/dev/null");
  if (!out) return 1;
  return 0;
}

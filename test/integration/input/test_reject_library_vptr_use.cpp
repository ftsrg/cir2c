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

// Issue #32: program code that uses the address of the std::basic_ios part of
// std::cout itself. The address needs the vptr of std::cout, whose vtable is in
// the compiled libc++: no code of the output sets that vptr, so cir2c stops,
// in both modes. (Without --externalize-std, the ostream-sink model has no
// virtual base, and its class does not convert to std::ios.)

// cir2c-test-expect-error-externalize-std: its vtable entries cannot be read (issue #32)

#include <iostream>

static long address(const std::ios *p) { return (long)p; }

int main() {
  return address(&std::cout) == 0 ? 1 : 0;
}

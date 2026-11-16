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

// Issue #17: libc++ calls the new handler again and again before operator new
// throws std::bad_alloc. The model of operator new does not, so a program that
// installs a new handler must not translate. std::set_new_handler is in the
// compiled library, so the definition check rejects it.

// cir2c-test-expect-error: std::set_new_handler

#include <new>

static void handler() { throw std::bad_alloc(); }

int main() {
  std::set_new_handler(handler);
  int *p = new int(3);
  int r = *p - 3;
  delete p;
  return r;
}

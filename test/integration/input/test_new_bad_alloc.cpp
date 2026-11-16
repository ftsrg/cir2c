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

// Issue #17: a throwing new expression never gives a null pointer. When malloc
// fails, it throws std::bad_alloc, and a handler can catch it. The nothrow form
// gives the null pointer, a new of 0 bytes gives distinct pointers, and an
// over-aligned type gets its alignment.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>

struct alignas(64) Wide { int x; };

// More than any address space: malloc fails. The argument keeps it at run time.
static std::size_t huge(int argc) {
  return (static_cast<std::size_t>(1) << 62) + static_cast<std::size_t>(argc);
}

int main(int argc, char **) {
  int r = 0;
  try {
    char *p = new char[huge(argc)];
    p[0] = 1;
    delete[] p;
    r += 100;
  } catch (const std::bad_alloc &) {
    r += 1;
  }
  char *q = new (std::nothrow) char[huge(argc)];
  if (q == nullptr) r += 2;
  char *a = new char[0];
  char *b = new char[0];
  if (a && b && a != b) r += 4;
  delete[] a;
  delete[] b;
  Wide *w = new Wide;
  if (reinterpret_cast<std::uintptr_t>(w) % 64 == 0) r += 8;
  delete w;
  Wide *ws = new Wide[3];
  if (reinterpret_cast<std::uintptr_t>(ws) % 64 == 0) r += 16;
  delete[] ws;
  std::printf("%d\n", r);
  return r == 31 ? 0 : 1;
}

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
#include <stdio.h>
#include <string.h>
#include <new>
// A defaulted member array is copied whole, not one element: the other bytes are not left as found.
struct A { unsigned char e[4]; };
struct S { A h{}; int x = 1; };
int main() {
  alignas(S) unsigned char buf[sizeof(S)];
  memset(buf, 0xAA, sizeof buf);
  S* s = new (buf) S;
  printf("%d %d %d %d %d\n", s->h.e[0], s->h.e[1], s->h.e[2], s->h.e[3], s->x);
  return 0;
}

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

// Issue #8: a C++ function can have the name of a C library function, as a
// separate overload. In C it must get another name: the output copies the
// initializer of `src` with the C library memcpy, which must not reach the
// function of the program.
static int calls = 0;

void *memcpy(void *dst, const void *src, int n) {
  ++calls;
  char *d = static_cast<char *>(dst);
  const char *s = static_cast<const char *>(src);
  for (int i = 0; i < n; ++i) d[i] = s[i];
  return dst;
}

int main() {
  const char src[4] = "abc";
  char dst[4];
  memcpy(dst, src, 4);
  if (calls != 1) return 1;
  return dst[2] == 'c' ? 0 : 2;
}

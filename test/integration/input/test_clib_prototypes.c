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

// Issue #12: the CIR has no const, so cir2c declared the C library functions as
// strlen(char *). Such a declaration is not compatible with the standard one,
// and that is undefined (C11 6.2.7p2). clang reports it with
// -Wincompatible-library-redeclaration.
//
// cir2c-test-cflags: -Werror=incompatible-library-redeclaration

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  char buf[32];
  const char *s = "hello";
  strcpy(buf, s);
  strncat(buf, " world", 3);
  char copy[32];
  memcpy(copy, buf, sizeof buf);
  memmove(copy + 1, copy, 4);
  const char *l = strchr(s, 'l');
  const char *r = strrchr(s, 'l');
  const char *w = strstr(buf, "wo");
  const void *e = memchr(s, 'e', 5);
  long n = strtol("42", NULL, 10);
  int i = atoi("7");
  int scanned = 0;
  sscanf("13", "%d", &scanned);
  char out[16];
  snprintf(out, sizeof out, "%d", (int)strlen(buf));
  printf("%s|%s|%s|%ld|%d|%d\n", buf, l, out, n, i, scanned);
  puts(copy);
  return (strcmp(buf, "hello wo") == 0 && memcmp(copy, "hhell", 5) == 0 &&
          r == s + 3 && w == buf + 6 && e == s + 1 && strspn(s, "leh") == 4 &&
          strcspn(s, "o") == 4 && strncmp(out, "8", 1) == 0) ? 0 : 1;
}

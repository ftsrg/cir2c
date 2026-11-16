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

static int counter = 2;
static int helper(void) { return 200; }
static const char *other = "yo";
extern const char *greeting;
int fa(void);
int main(void) {
  if (fa() != 101) return 1;
  if (fa() != 102) return 2;
  if (helper() + counter != 202) return 3;
  if (greeting[0] != 'h' || other[0] != 'y') return 4;
  return 0;
}

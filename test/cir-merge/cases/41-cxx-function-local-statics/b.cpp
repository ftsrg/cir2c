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

// cir-merge-cflags: -fno-threadsafe-statics
int seed() { return 10; }
static int f() { static int x = seed() * 2; return ++x; }
int fa();
int main() {
  if (fa() != 11 || fa() != 12) return 1;
  if (f() != 21) return 2;
  return 0;
}

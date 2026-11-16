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

// Issue #12: a static local variable with a dynamic initializer. With
// thread-safe statics, clang calls __cxa_guard_acquire and
// __cxa_guard_release of the C++ runtime, which the output does not define, so
// --no-externalize-std rejected the program. The initializer runs once. When
// it throws, the variable stays uninitialized and the next call runs the
// initializer again (C++20 [stmt.dcl]p4).

static int initCount = 0;

static int makeValue(bool fail) {
  ++initCount;
  if (fail)
    throw 1;
  return 40 + initCount;
}

static int get(bool fail) {
  static int value = makeValue(fail);
  return value;
}

int main() {
  int caught = 0;
  try {
    get(true);
  } catch (int) {
    caught = 1;
  }
  int a = get(false);
  int b = get(false);
  return (caught == 1 && a == 42 && b == 42 && initCount == 2) ? 0 : 1;
}

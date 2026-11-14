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

// The handler unit knows Derived, but only the other unit throws it. The handler
// of Base must catch it, at the offset of the Base subobject.
struct Other { virtual ~Other() {} long o = 5; };
struct Base { virtual ~Base() {} int b = 1; };
struct Derived : Other, Base { int d = 7; };
void thrower();
int touch() { Derived d; return d.d; }
int main() {
  try { thrower(); } catch (Base &e) { return e.b == 1 ? 0 : 2; }
  return 3;
}

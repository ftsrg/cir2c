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

// ns::X and ns__X need two different tags, so one of them gets a suffix. Another
// unit with only one of them would give it the plain tag: --tu-part must stop.
namespace ns { struct X { int a; }; }
struct ns__X { long b; };
int main() {
  ns::X x{1};
  ns__X y{2};
  return x.a + y.b == 3 ? 0 : 1;
}

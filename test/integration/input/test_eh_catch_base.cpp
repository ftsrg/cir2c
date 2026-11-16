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

// A handler for a class catches exceptions of its public subclasses: the
// generated C must agree with the native run on each of these.

struct Base { int code; Base(int c) : code(c) {} virtual ~Base() {} };
struct Mid : Base { Mid(int c) : Base(c) {} };
struct Leaf : Mid { Leaf(int c) : Mid(c) {} };
struct Other { int x; virtual ~Other() {} };
struct Multi : Other, Base { Multi(int c) : Base(c) { x = 0; } };
struct Hidden : private Base { Hidden() : Base(7) {} };

static void throw_leaf(int c) { throw Leaf(c); }

int catch_grandchild(int c) {
  try { throw_leaf(c); } catch (Base &b) { return b.code; }
  return -1;
}

int derived_handler_first(int c) {
  try { throw Leaf(c); } catch (Mid &) { return 1; } catch (Base &) { return 2; }
  return 0;
}

int base_handler_first(int c) {
  try { throw Mid(c); } catch (Leaf &) { return 1; } catch (Base &) { return 2; }
  return 0;
}

int second_base(int c) {
  try { throw Multi(c); } catch (Base &b) { return b.code; }
  return -1;
}

int private_base() {
  try { throw Hidden(); } catch (Base &) { return 1; } catch (...) { return 2; }
  return 0;
}

int main() {
  if (catch_grandchild(5) != 5) return 1;
  if (derived_handler_first(3) != 1) return 2;
  if (base_handler_first(3) != 2) return 3;
  if (second_base(9) != 9) return 4;
  if (private_base() != 2) return 5;
  return 0;
}

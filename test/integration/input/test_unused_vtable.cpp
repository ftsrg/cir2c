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

// Issue #12: when the key function of a class is defined inline, clang emits
// the vtable of the class as a linkonce_odr global, also when nothing creates
// an object of the class. The model prelude of issue #7 does this for the
// exception classes, so each program got their vtables and virtual functions.
// The output keeps such a vtable only when a kept function refers to it.
//
// cir2c-test-output-lacks: _ZTV6Unused
// cir2c-test-output-lacks: Unused__value
// cir2c-test-output-lacks: _ZTVSt9bad_alloc

struct Unused {
  virtual ~Unused();
  virtual int value() const;
};

inline Unused::~Unused() {}
inline int Unused::value() const { return 7; }

struct Used {
  virtual ~Used();
  virtual int value() const;
};

inline Used::~Used() {}
inline int Used::value() const { return 3; }

struct UsedDerived : Used {
  int value() const override { return 4; }
};

int main() {
  UsedDerived d;
  Used &u = d;
  return u.value() == 4 ? 0 : 1;
}

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


// Issue #9, part 6: the trivial assignment of an empty class copies nothing.
// libc++ stores an empty member with [[no_unique_address]], so it shares its
// address with another member: the comparator of a std::set with the size,
// the deleter of a std::unique_ptr with the pointer. A copy of the empty
// class must not change that other member.

#include <memory>
#include <queue>
#include <set>

struct M {
  int value;
  explicit M(int v) : value(v) {}
};

int main() {
  std::set<int> a{1, 2, 3}, b{4, 5};
  a.swap(b);
  if (a.size() != 2 || b.size() != 3) return 1;
  std::set<int> c;
  c = b;
  if (c.size() != 3) return 2;

  std::queue<std::unique_ptr<M>> q;
  for (int i = 0; i < 2; i++) q.push(std::unique_ptr<M>(new M(i + 10)));
  std::unique_ptr<M> out;
  int sum = 0;
  while (!q.empty()) {
    out = std::move(q.front());
    q.pop();
    sum += out->value;
    out.reset();
  }
  return sum == 21 ? 0 : 3;
}

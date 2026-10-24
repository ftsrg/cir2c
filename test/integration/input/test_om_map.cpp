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

// Test: map operations with operational models
#include <map>
#include <string>
#include <cassert>

using namespace std;

int main() {
    map<int, int> m;
    assert(m.size() == 0);
    assert(m.empty());

    m[1] = 10;
    m[2] = 20;
    m[3] = 30;
    assert(m.size() == 3);
    assert(m[1] == 10);
    assert(m[2] == 20);

    auto it = m.find(2);
    assert(it != m.end());

    assert(m.count(1) == 1);
    assert(m.count(99) == 0);

    m.erase(2);
    assert(m.size() == 2);
    assert(m.count(2) == 0);

    m.clear();
    assert(m.empty());

    map<int, int> m2;
    m2[100] = 1;
    m2[200] = 2;
    assert(m2.size() == 2);

    return 0;
}

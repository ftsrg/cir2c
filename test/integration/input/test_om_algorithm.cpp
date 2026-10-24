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

// Test: algorithm operations with operational models
#include <vector>
#include <algorithm>
#include <cassert>

using namespace std;

int main() {
    vector<int> v;
    v.push_back(3);
    v.push_back(1);
    v.push_back(4);
    v.push_back(1);
    v.push_back(5);

    sort(v.begin(), v.end());
    assert(v[0] == 1);
    assert(v[1] == 1);
    assert(v[2] == 3);
    assert(v[3] == 4);
    assert(v[4] == 5);

    assert(is_sorted(v.begin(), v.end()));

    auto it = find(v.begin(), v.end(), 4);
    assert(it != v.end());
    assert(*it == 4);

    int cnt = count(v.begin(), v.end(), 1);
    assert(cnt == 2);

    auto min_it = min_element(v.begin(), v.end());
    assert(*min_it == 1);

    auto max_it = max_element(v.begin(), v.end());
    assert(*max_it == 5);

    assert(binary_search(v.begin(), v.end(), 3));
    assert(!binary_search(v.begin(), v.end(), 99));

    reverse(v.begin(), v.end());
    assert(v[0] == 5);
    assert(v[4] == 1);

    int a = 3, b = 7;
    assert(max(a, b) == 7);
    assert(min(a, b) == 3);

    fill(v.begin(), v.end(), 0);
    for (size_t i = 0; i < v.size(); i++) {
        assert(v[i] == 0);
    }

    return 0;
}

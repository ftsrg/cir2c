// Test: vector operations with operational models
#include <vector>
#include <cassert>
#include <algorithm>

using namespace std;

int main() {
    vector<int> v;
    assert(v.size() == 0);
    assert(v.empty());

    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    assert(v.size() == 3);
    assert(v[0] == 1);
    assert(v[1] == 2);
    assert(v[2] == 3);
    assert(v.front() == 1);
    assert(v.back() == 3);

    v.pop_back();
    assert(v.size() == 2);
    assert(v.back() == 2);

    vector<int> v2(5, 10);
    assert(v2.size() == 5);
    for (size_t i = 0; i < v2.size(); i++) {
        assert(v2[i] == 10);
    }

    vector<int> v3 = v;
    assert(v3.size() == v.size());
    assert(v3[0] == v[0]);

    swap(v, v2);
    assert(v.size() == 5);
    assert(v2.size() == 2);

    sort(v.begin(), v.end());

    auto it = find(v.begin(), v.end(), 10);
    assert(it != v.end());

    v.clear();
    assert(v.empty());

    return 0;
}

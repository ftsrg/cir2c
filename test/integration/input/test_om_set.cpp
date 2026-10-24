// Test: set operations with operational models
#include <set>
#include <cassert>

using namespace std;

int main() {
    set<int> s;
    assert(s.size() == 0);
    assert(s.empty());

    s.insert(3);
    s.insert(1);
    s.insert(2);
    assert(s.size() == 3);

    assert(s.count(1) == 1);
    assert(s.count(99) == 0);

    auto it = s.find(2);
    assert(it != s.end());

    s.erase(1);
    assert(s.size() == 2);
    assert(s.count(1) == 0);

    s.clear();
    assert(s.empty());

    return 0;
}

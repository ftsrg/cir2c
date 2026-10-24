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

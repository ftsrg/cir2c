// Test: map operations with operational models
#include <map>
#include <cassert>

using namespace std;

int main() {
    map<int, string> m;
    assert(m.size() == 0);
    assert(m.empty());

    m[1] = "one";
    m[2] = "two";
    m[3] = "three";
    assert(m.size() == 3);
    assert(m[1] == "one");
    assert(m[2] == "two");

    auto it = m.find(2);
    assert(it != m.end());
    assert(it->second == "two");

    assert(m.count(1) == 1);
    assert(m.count(99) == 0);

    m.erase(2);
    assert(m.size() == 2);
    assert(m.count(2) == 0);

    m.clear();
    assert(m.empty());

    map<string, int> m2;
    m2.insert(make_pair("hello", 5));
    m2.insert(make_pair("world", 5));
    assert(m2.size() == 2);

    return 0;
}

// Test: string operations with operational models
#include <string>
#include <cassert>

using namespace std;

int main() {
    string s;
    assert(s.size() == 0);
    assert(s.empty());

    s = "hello";
    assert(s.size() == 5);
    assert(s[0] == 'h');
    assert(s[4] == 'o');

    string s2 = "world";
    string s3 = s + s2;
    assert(s3.size() == 10);

    s += "!";
    assert(s.size() == 6);
    assert(s[5] == '!');

    s2.clear();
    assert(s2.empty());

    s2 = "test";
    s2.push_back('!');
    assert(s2.size() == 5);

    string s4 = s2.substr(0, 4);
    assert(s4 == "test");

    assert(s.find("ell") != string::npos);
    assert(s.find("xyz") == string::npos);

    assert(s.compare("hello!") == 0);
    assert(s.compare("hello") > 0);

    swap(s, s2);
    assert(s == "test!");
    assert(s2 == "hello!");

    return 0;
}

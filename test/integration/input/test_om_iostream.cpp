// Test: iostream operations with operational models
#include <iostream>
#include <cassert>

using namespace std;

int main() {
    int x = 42;
    
    // cout should be a no-op in verification
    cout << "Hello, World!" << endl;
    cout << x << endl;
    cout << "value=" << x << endl;
    
    // cin should be a no-op
    int y;
    cin >> y;
    
    // cerr and clog should also work
    cerr << "error message" << endl;
    clog << "log message" << endl;
    
    assert(x == 42);
    
    return 0;
}

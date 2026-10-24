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

// Test: smart pointer operations with operational models
#include <memory>
#include <utility>
#include <cassert>

using namespace std;

struct Foo {
    int value;
    Foo(int v) : value(v) {}
};

int main() {
    // unique_ptr
    unique_ptr<Foo> p1(new Foo(42));
    assert(p1 != nullptr);
    assert(p1->value == 42);
    assert((*p1).value == 42);

    unique_ptr<Foo> p2 = move(p1);
    assert(p1 == nullptr);
    assert(p2->value == 42);

    p2.reset();
    assert(p2 == nullptr);

    // shared_ptr
    shared_ptr<Foo> s1(new Foo(100));
    assert(s1->value == 100);
    assert(s1.use_count() == 1);
    assert(s1.unique());

    shared_ptr<Foo> s2 = s1;
    assert(s1.use_count() == 2);
    assert(!s1.unique());
    assert(s2->value == 100);

    shared_ptr<Foo> s3;
    assert(s3 == nullptr);
    assert(s3.use_count() == 0);

    s3 = s1;
    assert(s1.use_count() == 3);

    // make_unique
    unique_ptr<int> mu = make_unique<int>();
    assert(*mu == 0);

    // make_shared
    shared_ptr<int> ms = make_shared<int>();
    assert(*ms == 0);

    return 0;
}

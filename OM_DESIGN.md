<!--
Copyright 2025 Budapest University of Technology and Economics

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# Operational Models Design Document

## Motivation

The cir2c tool translates C++ code to verifier-friendly C via ClangIR (CIR). By default, all `std::` calls are replaced with `__VERIFIER_nondet_*()` (nondeterministic), which is conservative but loses all library semantics. With `--no-externalize-std`, the real libc++ headers are used, but many symbols have no body in CIR, making verification unsound for containers.

Operational models solve this problem by providing simplified C++ headers that shadow the real libc++ headers during CIR generation. These models replace C++ templates, classes, and complex constructs with simple C-like structs and functions that are easy for Theta to verify.

## Architecture

```
C++ source (#include <vector>)
       ↓
clang++ -emit-cir -I libcxx-om/   ← Operational models shadow libc++
       ↓
CIR MLIR
       ↓
cir2c --externalize-std
       ↓
Verifier-friendly C with __VERIFIER_*
       ↓
Theta verification
       ↓
Result: SAFE / UNSAFE
```

## Design Decisions

### 1. Shadow Headers via --include

- Create a separate `libcxx-om/` directory
- Users pass `--include libcxx-om/` to `run-cir2c.sh`
- The OMs shadow the real libc++ headers during CIR generation
- Original headers remain intact and optional

### 2. Simplified Container Implementations

All containers use simple array-based implementations:
- **vector**: Dynamic array with push_back, pop_back, iterators
- **map/set**: Linear search arrays (not trees)
- **list**: Array-based (not linked list)
- **deque**: Array-based (not block-based)

This simplification makes the code easier to verify while preserving the essential semantics.

### 3. Bounded Sizes

All containers have a maximum size of 64 elements. This keeps verification tractable while being sufficient for most test cases.

### 4. Nondeterminism for I/O

- `cout`, `cin`, `cerr` are no-ops
- `rand()` returns `__VERIFIER_nondet_int()`
- `malloc()` returns `__VERIFIER_nondet_ptr()`
- File I/O functions return null/0

### 5. Assert Mapping

The `assert` macro is mapped to `__VERIFIER_error()`, so failed assertions become verification errors.

### 6. Smart Pointers

- `unique_ptr`: Simple wrapper with move semantics
- `shared_ptr`: Reference counting with a shared counter
- `weak_ptr`: Simplified, just stores a raw pointer

### 7. Algorithms

All algorithms use simple O(n²) implementations:
- `sort`: Insertion sort
- `find`: Linear search
- `binary_search`: Uses `lower_bound` with linear scan
- `merge`: Simple merge of two sorted arrays

## Verification Strategy

### Property-Based Verification

Write small C++ programs that use stdlib features, translate them through cir2c, and verify key properties:

```cpp
vector<int> v;
v.push_back(1);
v.push_back(2);
assert(v.size() == 2);  // Verified by Theta
```

### Interface Conformance

Verify that the C models satisfy the formal interface contracts from cppreference:
- `vector::push_back` increases size by 1
- `string::length` returns the correct value
- `map::find` returns end() for missing keys

## File Structure

```
libcxx-om/
├── README.md
├── c++/v1/
│   ├── vector          # std::vector
│   ├── string          # std::string
│   ├── map             # std::map
│   ├── set             # std::set
│   ├── unordered_map   # std::unordered_map
│   ├── unordered_set   # std::unordered_set
│   ├── list            # std::list
│   ├── deque           # std::deque
│   ├── array           # std::array
│   ├── algorithm       # std algorithms
│   ├── utility         # pair, swap, type traits
│   ├── functional      # function objects
│   ├── iostream        # cout, cin (no-ops)
│   ├── memory          # smart pointers
│   ├── numeric         # accumulate, etc.
│   ├── optional        # std::optional
│   ├── tuple           # std::tuple
│   ├── cassert         # assert
│   ├── cstring         # C string functions
│   ├── cmath           # C math functions
│   ├── cstdlib         # C stdlib functions
│   └── cstdio          # C stdio functions
```

## Known Limitations

1. **No templates in output**: All templates are resolved at compile time
2. **No exceptions**: No try/catch support
3. **No multithreading**: No mutex, thread, or atomic support
4. **No file I/O**: fstream is not modeled
5. **No regex, filesystem, coroutines**: Out of scope
6. **Bounded containers**: Max 64 elements
7. **Simple algorithms**: O(n²) implementations for verifiability

## Future Work

- Add more headers (fstream, sstream, regex)
- Add more algorithm variants
- Improve iterator implementations
- Add range-based for loop support
- Add more type traits

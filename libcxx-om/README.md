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

# libcxx-om: Operational Models for C++ Standard Library

Simplified C++ headers that shadow the real libc++ headers during CIR generation, enabling formal verification of C++ code that uses the standard library.

## Overview

The C++ standard library (libc++) contains 1721+ header files with complex templates, classes, and implementations that are difficult to verify formally. These operational models provide simplified versions of the most commonly used stdlib headers, replacing complex C++ constructs with simple, verification-friendly implementations.

## Design Principles

- **Verification-first**: Models prioritize verifiability over performance
- **Simplified semantics**: Complex operations are simplified (e.g., linear search instead of tree traversal)
- **Nondeterminism**: I/O and random operations use `__VERIFIER_nondet_*()` intrinsics
- **Bounded containers**: Containers have a maximum size (64 elements) to keep verification tractable
- **No external dependencies**: All models are self-contained header files

## Usage

Pass the `libcxx-om/` directory to `run-cir2c.sh` with the `--include` flag:

```bash
./run-cir2c.sh --include libcxx-om/ test/integration/input/test_om_vector.cpp
```

This causes clang to use the operational model headers instead of the real libc++ headers during CIR generation.

## Supported Headers

### Containers

| Header | Description |
|--------|-------------|
| `<vector>` | Dynamic array with push_back, pop_back, iterators, swap |
| `<string>` | String class with concatenation, find, substr, compare |
| `<map>` | Ordered associative container with insert, find, erase |
| `<set>` | Ordered set container with insert, find, erase |
| `<unordered_map>` | Hash-based map with insert, find, erase |
| `<unordered_set>` | Hash-based set with insert, find, erase |
| `<list>` | Doubly-linked list with push/pop, sort, merge |
| `<deque>` | Double-ended queue with push/pop front/back |
| `<array>` | Fixed-size array container |
| `<bitset>` | Fixed-size bit vector with bitwise operations |

### Algorithms and Utilities

| Header | Description |
|--------|-------------|
| `<algorithm>` | sort, find, count, min/max, reverse, copy, fill |
| `<numeric>` | accumulate, inner_product, partial_sum |
| `<optional>` | Optional value wrapper with has_value, value_or |
| `<tuple>` | Tuple types (up to 3 elements) |
| `<utility>` | pair, swap, type traits, move, forward |
| `<functional>` | function, default_delete, comparators |
| `<memory>` | unique_ptr, shared_ptr, weak_ptr, make_unique, make_shared |
| `<iterator>` | Iterator traits, begin/end, insert iterators |
| `<complex>` | Complex number arithmetic and functions |

### I/O

| Header | Description |
|--------|-------------|
| `<iostream>` | cout, cin, cerr as no-ops for verification |
| `<fstream>` | ifstream, ofstream, fstream (no-op I/O) |
| `<sstream>` | stringstream, ostringstream, istringstream |
| `<iomanip>` | setw, setprecision, hex, dec, manipulators |
| `<cstdio>` | printf, scanf, fopen, fclose (no-ops) |

### C Standard Library Wrappers

| Header | Description |
|--------|-------------|
| `<cassert>` | assert macro mapped to __VERIFIER_error |
| `<cstring>` | strlen, strcpy, strcmp, memcpy, memset |
| `<cmath>` | abs, sqrt, pow, sin, cos, floor, ceil |
| `<cstdlib>` | abs, rand, malloc, free, atoi, exit |
| `<ctime>` | time, clock, gmtime, localtime, difftime |
| `<cctype>` | isdigit, isalpha, tolower, toupper |
| `<cstddef>` | size_t, ptrdiff_t, nullptr_t, byte |
| `<cstdint>` | int8_t, int16_t, int32_t, int64_t (and unsigned) |
| `<climits>` | INT_MIN, INT_MAX, LONG_MIN, LONG_MAX |

### Type System

| Header | Description |
|--------|-------------|
| `<limits>` | std::numeric_limits for all fundamental types |
| `<typeinfo>` | std::type_info, typeid, bad_cast, bad_typeid |

### Exceptions

| Header | Description |
|--------|-------------|
| `<exception>` | std::exception base class with what() |
| `<stdexcept>` | logic_error, runtime_error, out_of_range, etc. |

### Random Number Generation

| Header | Description |
|--------|-------------|
| `<random>` | mt19937, uniform_int_distribution, normal_distribution |

## Limitations

- Containers are limited to 64 elements maximum
- Exception handling is simplified (no actual throw/catch semantics)
- No multithreading support
- File I/O uses no-op and nondeterministic stubs
- No regex, filesystem, or coroutine support
- Algorithms use simple O(n²) implementations for verifiability
- Random number generation uses nondeterminism for verification

## Testing

Test programs are in `test/integration/input/`:

- `test_om_vector.cpp` - Vector operations
- `test_om_string.cpp` - String operations
- `test_om_map.cpp` - Map operations
- `test_om_algorithm.cpp` - Algorithm operations
- `test_om_iostream.cpp` - I/O operations
- `test_om_smart_ptr.cpp` - Smart pointer operations

## License

Apache License 2.0, same as the cir2c project.

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
| `<algorithm>` | sort, find, count, min/max, reverse, copy, fill |
| `<iostream>` | cout, cin, cerr as no-ops for verification |
| `<memory>` | unique_ptr, shared_ptr, weak_ptr, make_unique, make_shared |
| `<numeric>` | accumulate, inner_product, partial_sum |
| `<optional>` | Optional value wrapper with has_value, value_or |
| `<tuple>` | Tuple types (up to 3 elements) |
| `<utility>` | pair, swap, type traits, move, forward |
| `<functional>` | function, default_delete, comparators |
| `<cassert>` | assert macro mapped to __VERIFIER_error |
| `<cstring>` | strlen, strcpy, strcmp, memcpy, memset |
| `<cmath>` | abs, sqrt, pow, sin, cos, floor, ceil |
| `<cstdlib>` | abs, rand, malloc, free, atoi, exit |
| `<cstdio>` | printf, scanf, fopen, fclose (no-ops) |

## Limitations

- Containers are limited to 64 elements maximum
- No exception handling
- No multithreading support
- No file I/O (fstream)
- No regex, filesystem, or coroutine support
- Algorithms use simple O(n²) implementations for verifiability

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

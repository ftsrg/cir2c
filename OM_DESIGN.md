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

libc++ is written for speed and ABI stability. Its containers are balanced trees, hash
tables and bit-tagged unions, and many of its members live in the compiled library, where
a verifier has no body for them. Operational models (OMs) are small replacements that
behave the same for a correct program and read as simple loops.

## How they plug in

```
./run-cir2c.sh --include libcxx-om/c++/v1 --no-externalize-std input.cpp out.c
```

`--include` becomes `-I`, which is searched before the `-isystem` libc++. Every
`#include`, including the ones inside libc++'s own headers, finds a model first if there
is one. `--no-externalize-std` keeps the bodies of the models.

## Design

**One namespace.** Models are declared in libc++'s own inline namespace
(`_LIBCPP_BEGIN_NAMESPACE_STD`), with the template parameters of `<__fwd/*.h>`. A model
is therefore the same entity libc++'s other headers name, so `<queue>` on the model of
`<deque>`, or `<algorithm>` on the model of `<vector>`, work unchanged.

**Shadow only what needs it.** A header is modelled when it is heavy or depends on the
compiled library. Everything else stays libc++, which keeps the API complete and the
models short: `<array>`, `<tuple>`, `<optional>`, `<utility>`, `<type_traits>`,
`<algorithm>` apart from sorting, `<memory>` with `unique_ptr` and `shared_ptr`,
`<chrono>`, `<atomic>`.

**The narrowest header.** When libc++ splits a class into an internal header that other
headers include directly, that internal header is the one shadowed, for example
`<__vector/vector.h>`. The models are tied to the libc++ of the pinned LLVM revision.

| Model | Shadows | How |
|---|---|---|
| `vector` | `__vector/vector.h` | heap array with `malloc`, raw pointers as iterators; `vector<bool>` stays libc++ |
| `string` | `string` | NUL-terminated heap array, no small-string union |
| `list`, `map`, `set`, `multimap`, `multiset` | same names | one circular linked list (`__om/node_list.h`, `__om/assoc.h`), linear search, stable references |
| `unordered_map`, `unordered_set`, `unordered_multi*` | same names | the same list, by equality; the hash is never computed |
| `deque` | `deque` | ring of pointers to single elements, stable references |
| `shared_ptr` counts | `__memory/shared_count.h` | libc++'s atomic counting, with bodies |
| `mutex` | `__mutex/mutex.h` | directly on `pthread_mutex_*` |
| `condition_variable` | `__condition_variable/condition_variable.h` | directly on `pthread_cond_*`; a timed wait may time out at any point |
| `thread` | `__thread/thread.h` | directly on `pthread_create` with a direct start routine |
| `function` | `__functional/function.h` | one heap object behind a virtual call |
| `sort` | `__algorithm/sort.h` | insertion sort, no compiled instantiations |
| `stdexcept` | `stdexcept` | message held by pointer |
| streams | `ios`, `ostream`, `istream`, `iostream`, `sstream`, `fstream`, `iomanip`, `streambuf`, `__ostream/basic_ostream.h` | `__om/iostream.h`, no locales or stream buffers |

## What the models do not preserve

- Complexity, iteration order of unordered containers, and capacity growth details.
- Allocators are accepted and ignored. Memory comes from `malloc`, which aborts on failure.
- `cout` and the other standard streams, and files, discard what is written. A file opens
  or fails nondeterministically and has no content to read.
- String streams keep real text, with base, width and fill. Floats print and parse only
  their whole part, and `to_string` of a float likewise.
- A `basic_iostream` holds two stream states, one per base, since a virtual `basic_ios`
  needs construction tables (VTTs) that cir2c does not lower yet.
- `hardware_concurrency()` is 1. Time passing is not modelled.
- Only C++17 and later: the models use `if constexpr` and `std::apply`.
- `recursive_mutex`, `timed_mutex` and `condition_variable_any` stay libc++, whose members
  have no bodies here.

## Testing

The models have to give the same results as a real standard library. A program that
exercises them is translated with and without the models and run, and its output is
compared with the program compiled natively.

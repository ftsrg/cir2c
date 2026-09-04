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

# cir2c

**cir2c translates C and C++ into verifier-friendly C**, so that formal
verification tools — model checkers and software verifiers that only speak
C — can be pointed at C++ programs, and at C programs using features their
own front ends handle poorly.

```
C / C++ source  ──clang -emit-cir──▶  CIR (ClangIR's MLIR dialect)  ──cir2c──▶  verifier-friendly C
```

The intermediate representation is [ClangIR](https://llvm.github.io/clangir/)
(CIR), an MLIR dialect produced by Clang. Compared to lowering all the way to
LLVM IR, CIR keeps the structure a verifier wants to see — explicit loops,
records, scopes, exception regions, virtual dispatch — while still having
decomposed the parts of C++ that a C-only tool cannot parse: templates,
overloads, constructors and destructors, RAII, name mangling.

The output is deliberately *not* a faithful C rendering of the input. It is a
model of the input, aimed at tools that will reason about it, and every place
where the model departs from C++ semantics is documented — see
[docs/limitations.md](docs/limitations.md). Read that page before trusting a
verdict obtained on generated code.

## Table of contents

- [Quick start](#quick-start)
- [Building from source](#building-from-source)
- [Usage](#usage)
- [What the generated C looks like](#what-the-generated-c-looks-like)
- [Documentation](#documentation)
- [Repository layout](#repository-layout)
- [License](#license)

## Quick start

Every push to `main` publishes a release containing statically linked binaries
— `cir2c` plus the matching ClangIR-enabled `clang`, `clang++` and `cir-opt`.
They depend only on libc, so there is nothing to build and nothing to install:

```bash
# Grab the latest release (needs the GitHub CLI, authenticated for this repo)
gh release download --repo ftsrg/cir2c --pattern 'cir2c-*.zip'
unzip cir2c-*.zip && cd output

# Translate a C++ program to verifier-friendly C
./clang++ -std=c++23 -stdlib=libc++ -nostdinc++ -isystem ./libcxx/include/c++/v1 \
          -S -emit-cir hello.cpp -o hello.mlir
./cir2c hello.mlir hello.c
```

For everyday use prefer the `run-cir2c.sh` wrapper from a source checkout — it
drives the whole pipeline in one command and sets those flags for you:

```bash
./run-cir2c.sh hello.cpp hello.c
```

## Building from source

You need a **ClangIR-enabled LLVM** — a stock distro LLVM will not do, because
`clang -emit-cir` and the CIR MLIR dialect are only built with
`-DCLANG_ENABLE_CIR=ON`. cir2c itself is a small CMake project on top of it.

The fastest route is the prebuilt toolchain image:

```bash
docker run --rm -it -v "$PWD:/src" -w /src ghcr.io/ftsrg/cir2c-llvm:latest \
    bash -c 'cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build'
```

To build the toolchain yourself instead (hours, not minutes):

```bash
./docker/build-llvm.sh --prefix ./llvm-install   # LLVM+Clang+MLIR+libc++, static
cmake -B build -DCMAKE_BUILD_TYPE=Release        # picks up ./llvm-install
cmake --build build
```

Full requirements, all three build routes, and how to point cir2c at an LLVM
you already have are in **[docs/building.md](docs/building.md)**.

### Which LLVM?

cir2c pins a **released** LLVM tag in [`llvm-version.txt`](llvm-version.txt)
(currently `llvmorg-23.1.0`) rather than tracking upstream `main`. Moving the
pin is a deliberate, twice-a-year action, not something upstream can force on
you mid-week. [docs/llvm-toolchain.md](docs/llvm-toolchain.md) explains the
version floor, what breaks below it, and how to move the pin.

## Usage

```bash
run-cir2c.sh [OPTIONS] <input-file> <output.c>
```

| Option | Purpose |
|---|---|
| `--lang c\|c++` | Force the input language instead of inferring it from the file extension. |
| `--std STD` | Language standard (default `c23` for C, `c++23` for C++). |
| `--flatten` | Run `cir-opt -cir-flatten-cfg` first, emitting flat (goto-based) C instead of structured C. |
| `--mlir FILE` | Also save the intermediate CIR to `FILE` (useful when debugging). |
| `--flat-mlir FILE` | Save the flattened CIR to `FILE` (implies `--flatten`). |
| `--include DIR` | Add `-I DIR` to the CIR-generation step (repeatable). |
| `--[no-]externalize-std` | Replace `std::` calls with nondeterministic havoc (default), or keep their bodies. See [docs/limitations.md](docs/limitations.md#externalizing-the-standard-library). |

Exit codes: `0` success, `2` `clang` failed, `3` `cir-opt` failed, `4` `cir2c`
failed. The full reference, including calling the `cir2c` binary directly on a
CIR module, is in **[docs/usage.md](docs/usage.md)**.

## What the generated C looks like

The output is plain C with a small, explicit vocabulary of verifier
intrinsics, all declared in the generated file:

| Construct | How it appears in the output |
|---|---|
| Uninitialised / havoc values | `__VERIFIER_nondet_<type>()`, `__VERIFIER_nondet_memory(p, n)` |
| Virtual dispatch | `__VERIFIER_virtual_call_<sig>(obj, slot, …)` over an emitted vtable |
| `throw` / `try` / `catch` | `__cir_exc_*` global state plus explicit dispatch on an RTTI tag |
| Static initialisation | Global constructors called explicitly at the top of `main` |
| C++ names | Demangled and sanitised (`ns::f(int)` → `ns__f_int`), collisions suffixed |

Everything the translation approximates is announced in a comment in the
generated file, so a reviewer reading the C can see where the model is weaker
than the source.

## Documentation

| Page | What is in it |
|---|---|
| [docs/building.md](docs/building.md) | Requirements and every supported way to build |
| [docs/usage.md](docs/usage.md) | CLI reference for `run-cir2c.sh` and `cir2c` |
| [docs/architecture.md](docs/architecture.md) | How the mapper works, and the C model for each C++ feature |
| [docs/llvm-toolchain.md](docs/llvm-toolchain.md) | LLVM version policy: what is pinned, why, and how to move it |
| [docs/limitations.md](docs/limitations.md) | Where the translation is an approximation — read before trusting a verdict |
| [test/README.md](test/README.md) | The test suites and how to run them |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Development workflow, coding conventions, CI |

## Repository layout

```
├── src/                  cir2c itself
│   ├── Mapper.*            dispatch, symbol/scope state, naming
│   ├── ModuleEmitter.cpp   module-level emission (functions, globals, vtables)
│   ├── TypeMapper.*        CIR types → C types
│   ├── ConstantEmitter.*   CIR constant attributes → C initialisers
│   ├── Traceability.*      per-operation MLIR↔C line mapping (--monitor-json)
│   └── handlers/           one file per family of CIR operations
├── test/                 integration tests and end-to-end corpora
├── docker/               toolchain image, static-binary image, build-llvm.sh
├── scripts/              build-static-binaries.sh, shared toolchain discovery
├── docs/                 the pages listed above
└── llvm-version.txt      the pinned LLVM revision
```

## License

Apache License 2.0 — see [LICENSE](LICENSE). Contributors are listed in
[CONTRIBUTORS.md](CONTRIBUTORS.md).

Binary releases bundle LLVM/Clang binaries, which are covered by the Apache
License 2.0 with LLVM Exceptions; that license ships alongside them as
`LICENSE.LLVM`.

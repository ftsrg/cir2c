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

# Building cir2c

cir2c is a small CMake project. Nearly all of the difficulty is in its one
dependency: a **ClangIR-enabled LLVM**. A distribution's `llvm-dev` package will
not work — ClangIR is only built when LLVM is configured with
`-DCLANG_ENABLE_CIR=ON`, and without it there is no `clang -emit-cir`, no
`cir-opt`, and no CIR MLIR dialect to link against.

There are three ways to get one. Pick by how much time you have:

| | Route | Time | Use when |
|---|---|---|---|
| A | [Prebuilt toolchain image](#a-prebuilt-toolchain-image) | minutes | You have Docker. **Start here.** |
| B | [Build the toolchain yourself](#b-build-the-toolchain-yourself) | 1–4 hours | No Docker, or you want a local LLVM tree to hack on |
| C | [Reuse an existing LLVM](#c-reuse-an-existing-llvm) | minutes | You already maintain a ClangIR build |

If all you want is to *run* cir2c, you do not need to build anything: every
push to `main` publishes static binaries. See [Binary releases](#binary-releases).

## Requirements

For cir2c itself:

- CMake ≥ 3.20
- A C++17 compiler
- Ninja or Make
- Git (the binary is stamped with the commit it was built from)
- A ClangIR-enabled LLVM ≥ **23.0** — see [llvm-toolchain.md](llvm-toolchain.md)

To build the LLVM toolchain (route B), additionally:

- `cmake`, `ninja`, `git`
- `clang` and `lld` (a GCC host compiler also works but is slower)
- Static zlib: `libz.a` + `zlib.h` (Debian/Ubuntu: `zlib1g-dev`)
- ~50 GB of free disk and, realistically, ≥ 16 GB of RAM

To run the test suites, additionally: `bash`, GNU `parallel`, `python3`.

## A. Prebuilt toolchain image

`ghcr.io/ftsrg/cir2c-llvm` carries the pinned LLVM, built statically and
installed at `/opt/cir`, together with libc++/libc++abi/libunwind. It is rebuilt
only when [`llvm-version.txt`](../llvm-version.txt) changes.

```bash
docker run --rm -it -v "$PWD:/src" -w /src ghcr.io/ftsrg/cir2c-llvm:latest bash

# inside the container — CMake finds /opt/cir on its own
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The image is published from this repository's packages; `docker login ghcr.io`
first if you have not pulled from it before.

## B. Build the toolchain yourself

[`docker/build-llvm.sh`](../docker/build-llvm.sh) builds LLVM+Clang+MLIR with
ClangIR, then libc++/libc++abi/libunwind against the clang it just built, all as
static archives in one prefix. The Dockerfile only calls this script, so running
it directly on a host produces the same toolchain.

```bash
./docker/build-llvm.sh --prefix ./llvm-install
```

| Flag | Meaning |
|---|---|
| `--prefix DIR` | Install prefix. Default `/opt/cir`; use `./llvm-install` for an in-tree build that CMake finds automatically. |
| `--src DIR` | `llvm-project` checkout to build from; cloned if missing. Default `./llvm-project`. |
| `--ref REF` | Tag/branch/commit to build. Default: the contents of `llvm-version.txt`. |
| `--jobs N` | Parallel jobs. Default `nproc`. |
| `--clean` | Delete the (multi-GB) build directories afterwards. Skip it to keep incremental rebuilds cheap. |

Then build cir2c:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

`./llvm-install` is one of the prefixes CMake probes automatically, so no `-D`
flag is needed if you used it. Both `./llvm-install/` and `./llvm-project/` are
gitignored.

### Why libc++ is built too

C++ input is compiled against libc++, not the system libstdc++. libstdc++ keeps
some container internals — `std::list`'s node-splice helpers, parts of
`std::string` and the locale machinery — out of line in compiled `.cc` units, so
ClangIR only ever sees a declaration and cir2c has no body to translate. libc++
keeps `list`/`map`/`set`/`deque` fully header-templated, so the bodies are there.
See [limitations.md](limitations.md#externalizing-the-standard-library).

## C. Reuse an existing LLVM

Point cir2c at a prefix:

```bash
cmake -B build -DCIR2C_LLVM_PREFIX=/path/to/llvm-install
```

or at the CMake package directories, if the layout is unusual:

```bash
cmake -B build \
  -DLLVM_DIR=/path/to/lib/cmake/llvm \
  -DMLIR_DIR=/path/to/lib/cmake/mlir
```

The scripts follow the same prefix through the environment:

```bash
export CIR2C_LLVM_PREFIX=/path/to/llvm-install
./run-cir2c.sh input.cpp output.c
```

With nothing set at all, CMake probes, in order: `./llvm-install`, `/opt/cir`,
then the system search path.

Configure fails with an explanation if the install is older than the version
floor, or if it has no ClangIR headers.

## Build options

| Option | Default | Meaning |
|---|---|---|
| `CIR2C_LLVM_PREFIX` | probed | Install prefix of the ClangIR toolchain |
| `LLVM_DIR` / `MLIR_DIR` | derived | CMake package dirs, if the prefix layout is unusual |
| `CMAKE_BUILD_TYPE` | *(empty)* | Use `Release` for anything but debugging |

The binary is written to `build/cir2c`; `cmake --install build` puts it in
`<prefix>/bin`.

## Verifying the build

```bash
build/cir2c --version              # prints the git commit it was built from
ctest --test-dir build --output-on-failure
```

`ctest` runs the integration suite: every program under `test/integration/input`
through the full clang → cir2c → clang pipeline. The larger corpora are opt-in;
see [../test/README.md](../test/README.md).

## Binary releases

Each push to `main` builds and publishes a zip containing statically linked
`cir2c`, `clang`, `clang++` and `cir-opt`, the Clang resource headers, the
libc++ headers and static archives, and the LLVM license. They depend only on
libc, so they run on any reasonably current Linux without installing LLVM.

```bash
gh release download --repo ftsrg/cir2c --pattern 'cir2c-*.zip'
unzip cir2c-*.zip && cd output
./cir2c --version
```

To reproduce that build locally:

```bash
./scripts/build-static-binaries.sh      # exports to ./output/
```

It compiles cir2c on top of the toolchain image and exports the binaries with
BuildKit; there is no LLVM rebuild, so it takes minutes. Set `DOCKER='sudo docker'`
if your user is not in the `docker` group.

## Troubleshooting

**`Could NOT find MLIR` / `Could NOT find LLVM`** — nothing was found at any
probed prefix. Pass `-DCIR2C_LLVM_PREFIX=…`, or build a toolchain (route B).

**`This LLVM install has no ClangIR headers`** — the install is real but was
built without `-DCLANG_ENABLE_CIR=ON`.

**`cir2c needs LLVM >= 23.0`** — see [llvm-toolchain.md](llvm-toolchain.md) for
what changed and why older releases cannot work.

**`libMLIRCIR not found`** — the LLVM prefix has the ClangIR headers but not the
compiled dialect library. Without it the CIR dialect is never registered and
every `cir.*` operation fails to parse. Check that the LLVM build completed and
installed, rather than only configuring.

**`no ClangIR-enabled clang found`** from `run-cir2c.sh` — the *script's* search
is separate from CMake's. Set `CIR2C_LLVM_PREFIX`, and `CIR2C_BIN` or
`CIR2C_BUILD_DIR` if the binary is not at `build/cir2c`.

**Undefined references when linking against a shared-library LLVM** — cir2c
links the CIR dialect by absolute path and, for static builds, hands the linker
the whole archive directory inside a `--start-group`. A `BUILD_SHARED_LIBS=ON`
LLVM is not exercised by CI; the pinned toolchain is fully static.

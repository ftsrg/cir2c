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

# The LLVM toolchain

cir2c is coupled to ClangIR, and ClangIR lives inside `llvm-project`. This page
says exactly which LLVM cir2c builds against, why that choice, and what to do
when it is time to move.

## What is pinned

One line, in [`llvm-version.txt`](../llvm-version.txt):

```
llvmorg-23.1.0
```

Everything reads that file: `docker/build-llvm.sh` clones it, the toolchain
image (`docker/llvm.Dockerfile`) bakes it in and copies it to
`/opt/cir/llvm-version.txt`, the static-binary export ships it next to the
binaries, and CI checks it against upstream. Changing the pin is a one-line
commit.

`CMakeLists.txt` separately enforces a **floor** of LLVM 23.0, so a build
against some other install fails at configure time with an explanation rather
than with a wall of template errors.

## Why a release tag, and not `main`

cir2c previously built against a shallow clone of `llvm-project`'s default
branch. That meant every upstream commit touching ClangIR or MLIR was a
potential build break, arriving unannounced, on somebody else's schedule — and
the toolchain image had to be rebuilt by hand to recover.

Nothing about cir2c requires that. It uses ClangIR the way a consumer does: it
parses CIR assembly and reads generated op/type/attribute accessors. Both are
present in released LLVM. Tracking `main` bought nothing except exposure.

Pinning a release tag changes the maintenance burden from *reactive and
unscheduled* to *deliberate and twice a year*:

|  | Tracking `main` | Pinned release tag |
|---|---|---|
| Breakage arrives | whenever upstream lands a CIR change | only when you move the pin |
| Toolchain image | rebuilt to recover from breakage | rebuilt when the pin moves |
| Reproducing an old result | needs the LLVM commit hash, if anyone recorded it | the tag is in the repo and in the release |
| New ClangIR features | immediately | at the next LLVM release |

The cost is the last row: a CIR operation added upstream after the pinned
release is not available until the pin moves. In practice ClangIR features land
faster than cir2c can map them, so this has not been the binding constraint.

## Why LLVM 23 is the floor

ClangIR is upstreamed incrementally, and the operations cir2c maps arrived over
several releases. cir2c uses **150 distinct CIR operations**. Comparing that
set against each release's `CIROps.td`:

| LLVM release | CIR ops cir2c needs that are missing |
|---|---|
| 21.1.x | ~all — arithmetic, exceptions, vtables, complex, math builtins |
| 22.1.x | 60, including every arithmetic op |
| **23.1.0** | **0** |

LLVM 22 and earlier model arithmetic as one `cir.binop` carrying a kind
attribute; LLVM 23 splits it into `cir.add`, `cir.sub`, `cir.mul` and friends,
which is what cir2c's handlers are written against. Exception handling
(`cir.eh.dispatch`, `cir.begin_catch`, `cir.eh.terminate`), `cir.cleanup_scope`
and `cir.vtable.get_type_info` also only appear in 23.

Beyond the operation *names*, all 23 of cir2c's translation units compile
cleanly against `llvmorg-23.1.0`'s generated ClangIR headers, so the accessor
APIs match too.

There is one thing to know about that check: it also caught a rename that had
already broken cir2c against current upstream `main`. `cir.func`'s special-member
attribute slot, read as `getCxxSpecialMember()`, is now `func_info` /
`getFuncInfo()`. The tree could not be built against `main` any more. That is
the failure mode this pin exists to prevent, and it is fixed in the same change
that introduced the pin.

## Moving the pin

Roughly every six months, when a new LLVM major release ships:

1. Edit `llvm-version.txt` to the new tag (e.g. `llvmorg-24.1.0`).
2. Rebuild the toolchain image, either by running the **Build LLVM toolchain
   image** workflow from the Actions tab, or locally:
   ```bash
   docker build -f docker/llvm.Dockerfile -t ghcr.io/ftsrg/cir2c-llvm:latest .
   ```
3. Build cir2c against it and run the test suites (see [building.md](building.md)
   and [../test/README.md](../test/README.md)).
4. Fix whatever moved. Renamed accessors show up as compile errors; changed
   assembly syntax shows up as parse failures in the integration suite.
5. If the new release drops something cir2c relied on, raise
   `CIR2C_LLVM_MIN_VERSION` in `CMakeLists.txt` in the same commit.

The **Check for a newer LLVM release** workflow runs weekly, compares the pin
against upstream's tags, and opens an issue when a newer release exists — so
the six-monthly step is a reminder that arrives on its own rather than
something to remember.

## Using an LLVM you already have

cir2c does not insist on the pinned build; it insists on *a* ClangIR-enabled
LLVM at or above the floor. Point it at yours with:

```bash
cmake -B build -DCIR2C_LLVM_PREFIX=/path/to/llvm-install
# or, if the CMake packages are somewhere unusual:
cmake -B build -DLLVM_DIR=/path/to/lib/cmake/llvm -DMLIR_DIR=/path/to/lib/cmake/mlir
```

The same prefix works for the scripts:

```bash
export CIR2C_LLVM_PREFIX=/path/to/llvm-install
./run-cir2c.sh input.cpp output.c
```

Configure-time checks will tell you if that install predates the floor, or was
built without `-DCLANG_ENABLE_CIR=ON`.

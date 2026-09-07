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

cir2c translates C and C++ programs into verifier-friendly C. Formal
verification tools that read only C can then examine C++ programs. They can also
examine C programs that their own front ends do not process correctly.

```
C or C++ source  --clang -emit-cir-->  CIR  --cir2c-->  verifier-friendly C
```

CIR is the ClangIR dialect of MLIR. Clang makes the CIR from the source code.

## Cite this work

**Note: The entry that follows is a placeholder. Replace it when the
publication is available.**

```bibtex
@misc{cir2c,
  author       = {Bajczi, Levente and {\'A}d{\'a}m, Zs{\'o}fia},
  title        = {{cir2c}: A {ClangIR}-based Translator from {C}/{C++}
                  to Verifier-Friendly {C}},
  year         = {2026},
  howpublished = {\url{https://github.com/ftsrg/cir2c}}
}
```

Give also the version that you used. The `cir2c --version` command prints the
commit. Each release contains the `llvm-version.txt` file, which gives the LLVM
revision.

## Contents

- [Cite this work](#cite-this-work)
- [1. Description](#1-description)
- [2. Get the binaries](#2-get-the-binaries)
- [3. Requirements](#3-requirements)
- [4. Build](#4-build)
- [5. Operation](#5-operation)
- [6. The LLVM toolchain](#6-the-llvm-toolchain)
- [7. Design](#7-design)
- [8. Limits](#8-limits)
- [9. Tests](#9-tests)
- [10. Contribute](#10-contribute)
- [11. License](#11-license)

---

## 1. Description

cir2c uses CIR as its input. CIR keeps the structure that a verification tool
must have. It keeps loops, records, scopes, exception regions and virtual
dispatch. At the same time, Clang removes the C++ constructions that a C tool
cannot read. Clang removes templates, overloads, constructors, destructors,
RAII and name mangling.

LLVM IR is not sufficient for this task. LLVM IR removes too much. Loops become
branches, and records become byte offsets.

**WARNING: The output is a model of the input program. The output is not an
equivalent program.** Some parts of C++ have no equivalent in C. In these
parts, cir2c makes an approximation. Read [8. Limits](#8-limits) before you
accept a result from a verification tool.

The output is C code with a small set of verifier intrinsics. cir2c declares all
of these intrinsics in the output file.

| Construction | Form in the output |
|---|---|
| Unknown value | `__VERIFIER_nondet_<type>()`, `__VERIFIER_nondet_memory(p, n)` |
| Virtual dispatch | `__VERIFIER_virtual_call_<sig>(obj, slot, …)` with a vtable |
| `throw`, `try`, `catch` | The `__cir_exc_*` global variables and a comparison of the RTTI tag |
| Static initialization | Calls to the global constructors at the start of `main` |
| C++ names | Demangled and made safe: `ns::f(int)` becomes `ns__f_int` |

cir2c writes a comment in the output file for each approximation that it makes.

---

## 2. Get the binaries

Each push to the `main` branch makes a release. The release contains static
binaries: `cir2c`, `clang`, `clang++` and `cir-opt`. The binaries need only
libc. You do not have to build or install LLVM.

```bash
gh release download --repo ftsrg/cir2c --pattern 'cir2c-*.zip'
unzip cir2c-*.zip           # Makes the cir2c-<commit>/ directory
cd cir2c-*/
```

Each archive unpacks into a directory with the name of the release. Thus you can
keep more than one release at the same time.

The archive contains these items:

| Item | Contents |
|---|---|
| `cir2c`, `clang`, `clang++`, `cir-opt` | The static binaries |
| `include/` | The Clang resource headers |
| `libcxx/` | The libc++ headers and the static libraries |
| `llvm-version.txt` | The LLVM revision of the build |
| `LICENSE`, `LICENSE.LLVM` | The licenses |

To translate a C++ program with these binaries, do the steps that follow:

```bash
./clang++ -std=c++23 -stdlib=libc++ -nostdinc++ -isystem ./libcxx/include/c++/v1 \
          -S -emit-cir hello.cpp -o hello.mlir
./cir2c hello.mlir hello.c
```

If you have a source checkout, use the `run-cir2c.sh` script. The script does
all the steps and sets these flags for you. Refer to
[5. Operation](#5-operation).

---

## 3. Requirements

To build cir2c, you must have these items:

| Item | Version |
|---|---|
| CMake | 3.20 or later |
| A C++ compiler | C++17 or later |
| Ninja or Make | Any |
| Git | Any |
| An LLVM with ClangIR | 24.0 or later |

**CAUTION: A standard LLVM package is not sufficient.** The LLVM build must
have the `-DCLANG_ENABLE_CIR=ON` option. Without this option, there is no
`clang -emit-cir` command, no `cir-opt` command and no CIR dialect.

To build the LLVM toolchain, you must also have these items:

- `cmake`, `ninja` and `git`
- `clang` and `lld` (a GCC host compiler operates, but it is slower)
- A static zlib: the `libz.a` file and the `zlib.h` file
  (on Debian and Ubuntu, install the `zlib1g-dev` package)
- 50 GB of free disk space and 16 GB of memory

To run the tests, you must also have `bash`, GNU `parallel` and `python3`.

---

## 4. Build

There are three methods. Select a method from the table.

| Method | Time | Select this method when |
|---|---|---|
| [A. Use the toolchain image](#4a-use-the-toolchain-image) | Minutes | You have Docker |
| [B. Build the toolchain](#4b-build-the-toolchain) | 1 to 4 hours | You do not have Docker |
| [C. Use your own LLVM](#4c-use-your-own-llvm) | Minutes | You have a ClangIR build |

### 4A. Use the toolchain image

The `ghcr.io/ftsrg/cir2c-llvm` image contains the pinned LLVM. The image
installs LLVM in the `/opt/cir` directory. The image also contains libc++,
libc++abi and libunwind.

```bash
docker run --rm -it -v "$PWD:/src" -w /src ghcr.io/ftsrg/cir2c-llvm:latest bash

# In the container. CMake finds /opt/cir automatically.
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

If you did not use this registry before, first do the `docker login ghcr.io`
command.

### 4B. Build the toolchain

The `docker/build-llvm.sh` script builds LLVM, Clang and MLIR with ClangIR. The
script then builds libc++, libc++abi and libunwind. The script makes static
libraries and installs all of them in one prefix.

```bash
./docker/build-llvm.sh --prefix ./llvm-install
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

CMake finds the `./llvm-install` directory automatically.

| Option | Function |
|---|---|
| `--prefix DIR` | The install prefix. The default is `/opt/cir`. |
| `--src DIR` | The `llvm-project` checkout. The default is `./llvm-project`. The script clones it if it is not there. |
| `--ref REF` | The revision to build. The default is the contents of `llvm-version.txt`. |
| `--jobs N` | The number of parallel jobs. The default is `nproc`. |
| `--clean` | Delete the large build directories at the end. |

**Note: The script also builds libc++.** Clang compiles C++ input with libc++
and not with libstdc++. libstdc++ keeps some container functions in compiled
`.cc` files. ClangIR sees only a declaration for these functions. libc++ keeps
`list`, `map`, `set` and `deque` fully in the headers. Refer to
[8.2 The standard library](#82-the-standard-library).

### 4C. Use your own LLVM

Give CMake the prefix of your LLVM installation:

```bash
cmake -B build -DCIR2C_LLVM_PREFIX=/path/to/llvm-install
```

If the directory layout is different, give CMake the two package directories:

```bash
cmake -B build \
  -DLLVM_DIR=/path/to/lib/cmake/llvm \
  -DMLIR_DIR=/path/to/lib/cmake/mlir
```

The scripts read the same prefix from the environment:

```bash
export CIR2C_LLVM_PREFIX=/path/to/llvm-install
./run-cir2c.sh input.cpp output.c
```

If you set no variable, CMake examines these locations in this sequence:
`./llvm-install`, then `/opt/cir`, then the system search path.

### 4D. Build options

| Option | Default | Function |
|---|---|---|
| `CIR2C_LLVM_PREFIX` | Found automatically | The prefix of the ClangIR toolchain |
| `LLVM_DIR`, `MLIR_DIR` | Made from the prefix | The CMake package directories |
| `CMAKE_BUILD_TYPE` | Empty | Use `Release` for all work except debug work |

CMake writes the binary to `build/cir2c`. The `cmake --install build` command
copies the binary to `<prefix>/bin`.

### 4E. Verify the build

```bash
build/cir2c --version              # Prints the Git commit of the build
ctest --test-dir build --output-on-failure
```

`ctest` runs the integration tests. Refer to [9. Tests](#9-tests).

### 4F. Make the static binaries

```bash
./scripts/build-static-binaries.sh      # Writes to the ./output directory
```

The script compiles cir2c on the toolchain image and exports the binaries. The
script does not build LLVM again. Thus the script is quick.

If your user account is not in the `docker` group, set `DOCKER='sudo docker'`.

### 4G. Correct build problems

| Message | Cause and correction |
|---|---|
| `Could NOT find MLIR` or `Could NOT find LLVM` | CMake found no toolchain. Give the `-DCIR2C_LLVM_PREFIX=…` option, or use method B. |
| `This LLVM install has no ClangIR headers` | The LLVM build did not have the `-DCLANG_ENABLE_CIR=ON` option. |
| `cir2c needs LLVM >= 24.0` | The LLVM is too old. Refer to [6. The LLVM toolchain](#6-the-llvm-toolchain). |
| `libMLIRCIR not found` | The prefix has the headers but not the library. Without the library, all `cir.*` operations fail to parse. Make sure that the LLVM build completed and installed. |
| `no ClangIR-enabled clang found` | The scripts do their own search. Set `CIR2C_LLVM_PREFIX`. Set also `CIR2C_BIN` or `CIR2C_BUILD_DIR` if the binary is not at `build/cir2c`. |

**Note: A shared-library LLVM is not tested.** cir2c links the CIR dialect by
absolute path. For a static build, cir2c gives the full library directory to the
linker in one `--start-group` list. The pinned toolchain is fully static.

---

## 5. Operation

### 5.1 The run-cir2c.sh script

The `run-cir2c.sh` script is the recommended interface. The script does all four
steps: `clang -emit-cir`, then preprocessing, then optional flattening, then
`cir2c`.

```bash
./run-cir2c.sh input.c   output.c     # A C program
./run-cir2c.sh input.cpp output.c     # A C++ program
```

The script makes the output directory if it is not there.

```
run-cir2c.sh [OPTIONS] <input-file> <output.c>
```

| Option | Function |
|---|---|
| `--lang c\|c++` | Set the input language. The default comes from the file extension. The `.cpp`, `.cxx`, `.cc` and `.C` extensions are C++. |
| `--std STD` | Set the language standard. The default is `c23` for C and `c++23` for C++. |
| `--flatten` | Do the `cir-opt -cir-flatten-cfg` step first. This makes flat C code with `goto` statements. |
| `--mlir FILE` | Write the intermediate CIR to `FILE`. Use this option to find problems. |
| `--flat-mlir FILE` | Write the flat CIR to `FILE`. This option includes `--flatten`. |
| `--include DIR` | Add the `-I DIR` option to the CIR step. You can give this option more than one time. |
| `--externalize-std` | Replace the `std::` calls and the library calls with unknown values. This is the default. |
| `--no-externalize-std` | Keep the bodies of the `std::` functions. **CAUTION: Read [8.2 The standard library](#82-the-standard-library) first.** |

| Exit code | Meaning |
|---|---|
| 0 | The operation is successful. |
| 2 | `clang` failed. The source has an error, or ClangIR cannot process a construction. |
| 3 | `cir-opt` failed during the `--flatten` step. |
| 4 | `cir2c` failed. Usually the CIR contains an unsupported operation. |

Exit code 2 is important. It shows that Clang made no CIR. This is a limit of
ClangIR and not a limit of cir2c.

### 5.2 Structured output and flat output

By default, cir2c reads structured CIR. The `cir.for`, `cir.while`, `cir.if`,
`cir.scope` and `cir.try` operations are still regions. The output C code has
`for`, `while` and `if` statements and correct scopes. Use this default. A
verification tool examines structured control flow more efficiently. The output
is also easier to read.

The `--flatten` option changes all of these operations into basic blocks and
branches. The output C code then has labels and `goto` statements. Use this
option when a construction does not translate in the structured form.

### 5.3 How the scripts find the tools

The scripts examine these locations for `clang`, `clang++` and `cir-opt`, in
this sequence:

1. The `$CIR2C_LLVM_PREFIX` directory
2. The `<repository>/llvm-install` directory
3. The `/opt/cir` directory
4. The `PATH` variable

The scripts examine these locations for the `cir2c` binary:

1. The `$CIR2C_BIN` file
2. The `${CIR2C_BUILD_DIR:-<repository>/build}/cir2c` file

If the scripts find no tool, they print the locations that they examined.

### 5.4 The cir2c binary

If you have a CIR module, you can start the binary directly. You must then make
the CIR and preprocess it yourself.

```
cir2c [--monitor-json <trace.json>] [--[no-]externalize-std] <input.mlir> <output.c>
cir2c --version
```

| Argument | Function |
|---|---|
| `<input.mlir>` | A CIR module from `clang -S -emit-cir`, with an optional `cir-opt -cir-flatten-cfg` step |
| `<output.c>` | The path of the output C file |
| `--version` | Print the Git commit of the build and stop |
| `--monitor-json FILE` | Write a JSON trace. The trace maps each CIR operation to the C lines that it made. |
| `--[no-]externalize-std` | The same function as in the script. The default is `--externalize-std`. |

| Exit code | Meaning |
|---|---|
| 0 | The operation is successful. |
| 2 | The arguments are not correct. |
| 3 | cir2c cannot open the input file. |
| 4 | The MLIR did not parse. |
| 5 | cir2c cannot open the output file. |
| 6 | The translation failed. |

The `--monitor-json` option records the operation text, the C code and the line
numbers on the two sides. Use this trace to build tools that show a source
construction and its translation together. Use it also to examine a result from
a verification tool. cir2c calculates the line map only when you give this
option, because the calculation is slow.

### 5.5 Examine a failure

1. Keep the intermediate CIR:
   ```bash
   ./run-cir2c.sh --mlir /tmp/case.mlir case.cpp /tmp/case.c
   ```
2. If the exit code is 2, the problem is in Clang. Clang made no CIR for that
   source. Read the Clang messages.
3. If the exit code is 4, the CIR is in `/tmp/case.mlir`. cir2c prints the name
   of the operation that it cannot translate. Find that name in the `.mlir` file.
   The handlers are in the `src/handlers/` directory.
4. If cir2c made C code but the code is not correct, start the binary again on
   the `.mlir` file. This removes Clang and `cir-opt` from the sequence and makes
   the work cycle quick.
5. Try the `--flatten` option and compare the two results. A construction that
   fails in one mode can operate in the other mode.

---

## 6. The LLVM toolchain

### 6.1 The pinned revision

The `llvm-version.txt` file contains one revision:

```
945e3e825b3ee88a8c1fed0da2dc88d6b2114e37
```

The revision is immutable. It is a release tag, or a full 40-character commit
SHA. It is never a branch name. A branch moves, and a moving toolchain changes
the result of a build of unchanged sources.

All parts of the repository read this file. The `docker/build-llvm.sh` script
gets this revision. The toolchain image contains this revision and copies the
file to `/opt/cir/llvm-version.txt`. The static binaries include the file. The
toolchain image uses the revision as its tag. A weekly workflow compares the
file with the upstream releases.

To change the revision, change one line. Refer to
[6.6 Change the pin](#66-change-the-pin).

`CMakeLists.txt` also sets a minimum version of LLVM 24.0. A build against a
different installation stops at the configuration step with a clear message.

### 6.2 Why a pinned revision and not the main branch

Before, the build used a shallow clone of the default branch of `llvm-project`.
Each upstream commit that changed ClangIR or MLIR could break the build. The
break came with no warning. A person then had to build the toolchain image
again.

| Item | The main branch | A pinned revision |
|---|---|---|
| When a break occurs | At each upstream ClangIR change | Only when you change the pin |
| The toolchain image | Built again to correct a break | Built again when the pin changes |
| To repeat an old result | You must know the LLVM commit | The revision is in the repository |
| New ClangIR functions | Immediately | When you change the pin |

The last row is the cost. A CIR operation that upstream adds after the pinned
revision is not available until a person moves the pin.

### 6.3 Why the pin is a commit and not a release tag

A release tag is better than a commit SHA. A tag is easier to read, and a
release gets patch versions. Use a tag when you can.

This pin is a commit because a usable release does not exist yet:

| Candidate | Why it is not used |
|---|---|
| `llvmorg-23.1.0` | It has no `cir.fmuladd`, and it has the removed special member accessor. |
| `llvmorg-24-init` | This tag is the start of the LLVM 24 cycle, not a release. It has the same limits as LLVM 23. |
| An LLVM 24 release | It does not exist. Upstream has not made one. |

The pinned commit is `llvm-project` main in the LLVM 24 cycle. It is still one
immutable revision, so it gives all the properties in the table of
[6.2](#62-why-a-pinned-revision-and-not-the-main-branch). Only the frequency of
the security patches and the quality of the release notes are different.

The **Check for a newer LLVM release** workflow knows this condition. While the
pin is a commit, the workflow makes an issue only when a release becomes equal
to or newer than the pinned LLVM version. Then a person can move the pin back to
a tag.

### 6.4 Why LLVM 24 is the minimum

Upstream adds ClangIR in steps. cir2c uses 150 different CIR operations. This
table shows what each release does not have:

| LLVM release | What cir2c cannot get from it |
|---|---|
| 21.1.x | Almost all operations: arithmetic, exceptions, vtables, complex numbers, math |
| 22.1.x | 60 operations, and this includes all arithmetic operations |
| 23.1.0 | `cir.fmuladd`, `cir.fma`, and the current `func_info` attribute |
| **24 (the pin)** | **Nothing** |

LLVM 22 and earlier releases use one `cir.binop` operation with a kind
attribute. LLVM 23 makes separate `cir.add`, `cir.sub` and `cir.mul`
operations. The cir2c handlers use the separate operations. Exception handling,
`cir.cleanup_scope` and `cir.vtable.get_type_info` are also new in LLVM 23.

LLVM 24 makes two changes that cir2c must obey:

- Clang emits one `cir.fmuladd` operation for `a * b + c` when the default
  `-ffp-contract=on` is active. Before, Clang emitted `cir.fmul` and `cir.fadd`.
  A mapper with no handler for `cir.fmuladd` stops with an error on usual
  floating-point code.
- The special member slot of `cir.func` is now the more general `func_info`
  attribute, and MLIR made the symbol names inherent attributes. Thus
  `getCxxSpecialMember()` and `mlir::SymbolTable::getSymbolAttrName()` do not
  exist. cir2c uses `isCxxSpecialAssignment()` with
  `isCxxTrivialMemberFunction()`, and a null-safe `symbolNameAttr()` helper.

All source files of cir2c compile against the ClangIR headers of the pinned
revision. Thus the accessor names also agree.

### 6.5 How CI waits for the toolchain image

The **LLVM toolchain image** workflow publishes the image with the pinned
revision as its tag, for example `ghcr.io/ftsrg/cir2c-llvm:945e3e82...`. The
**CI** workflow and the **Release** workflow pull that exact tag. They do not
pull `latest`.

This is necessary. A build of LLVM takes hours. In that time, `latest` continues
to point to the previous toolchain. A job that used `latest` after a pin change
would build the new sources against the old LLVM. Its result would be true for
neither.

Thus each LLVM-dependent job has a `Toolchain image` step before it:

| Condition | What occurs |
|---|---|
| The image for the pin exists | The jobs run against it. |
| The image does not exist, on a push to `main` | The jobs stop and wait. When the image build is successful, CI and Release start again for the same commit. |
| The image does not exist, in a pull request | The `Toolchain image` job fails and tells you to build the image. |

The checks that do not use LLVM (the license headers, the shell scripts and the
commit messages) always run.

Only `main` moves the `latest` tag. Thus you can prepare a pin change on a
branch, and build its toolchain image, and not change what other persons pull.

### 6.6 Change the pin

Do these steps when you move to a newer LLVM:

1. Change `llvm-version.txt` to the new revision. Use a release tag if a usable
   release exists. If not, use a full commit SHA. Refer to
   [6.3](#63-why-the-pin-is-a-commit-and-not-a-release-tag).
2. Build the toolchain image again. Start the **LLVM toolchain image** workflow
   from the Actions page. Start it on your branch, not on `main`, while you
   prepare the change. As an alternative, do this command:
   ```bash
   docker build -f docker/llvm.Dockerfile -t ghcr.io/ftsrg/cir2c-llvm:latest .
   ```
3. Build cir2c against the new image and run the tests.
4. Correct the errors. Changed accessor names cause compilation errors. Changed
   assembly syntax causes parse errors in the integration tests. A new operation
   that replaces an operation that cir2c maps needs a new handler.
5. If the new revision removes a function that cir2c uses, increase
   `CIR2C_LLVM_MIN_VERSION` in `CMakeLists.txt` in the same commit.

The **Check for a newer LLVM release** workflow operates each week. It makes an
issue when a better pin becomes available.

---

## 7. Design

### 7.1 The sequence

```
  input.cpp
     |  clang -x c++ -std=c++23 -S -emit-cir  (-stdlib=libc++)
     v
  CIR module (MLIR text)
     |  preprocessing of the qualifiers
     v
  CIR module        --optional-->  cir-opt -cir-flatten-cfg  -->  flat CIR
     |                                                              |
     v                                                              v
  cir2c: structured output                            cir2c: goto output
     |                                                              |
     +---------------------->  output.c  <--------------------------+
```

Each step is a different process. Each intermediate result is text that you can
read. This is intentional. When a translation is not correct, you can keep the
CIR with the `--mlir` option, change it, and give it to `cir2c` again.

### 7.2 The components

cir2c does not build CIR. cir2c reads CIR. The `main.cpp` file registers the CIR
dialect with an MLIR context and parses the input module. All subsequent work is
an examination of `mlir::Operation` objects.

| Component | Function |
|---|---|
| `Mapper` (`Mapper.h`, `Mapper.cpp`) | The handler table, the map from SSA values to C expressions, the scope and loop stacks, the symbol names, and the set of necessary intrinsics |
| `ModuleEmitter.cpp` | The module output: the sequence of globals, function declarations, vtables and function bodies |
| `TypeMapper` | CIR types to C types. `!cir.int<s, 32>` becomes `int`. `!cir.ptr<T>` becomes `T*`. Records become `struct` types. |
| `ConstantEmitter` | CIR constant attributes to C initializers, and this includes arrays and records |
| `Traceability` | The map from operations to C lines for the `--monitor-json` option |
| `handlers/` | One file for each group of CIR operations |

### 7.3 The handlers

Each handler has the name of a CIR operation, for example `cir.alloca`,
`cir.for` or `cir.throw`. The handlers are in groups by subject:
`ArithmeticHandlers`, `ControlFlowHandlers`, `ExceptionHandlers`,
`VTableHandlers`, `MemoryHandlers`, `RecordHandlers`, `CallHandlers`,
`CastHandlers`, `ComplexHandlers`, `FloatMathHandlers`, `BuiltinHandlers`,
`ConstantHandlers`, `GlobalHandlers` and `VariadicHandlers`.

Each group registers itself at start-up with the `REGISTER_HANDLER_MODULE`
macro. To add a group, add a `.cpp` file to `CMakeLists.txt`. No other change is
necessary.

The handlers have types. The
`Mapper::registerTypedHandler<cir::LoadOp>(...)` function takes the operation
name from the generated class. The handler receives an operation of the correct
type. Thus the handler code uses `cir::LoadOp` and not a raw `Operation*`.

**WARNING: An unknown operation is always an error.** cir2c prints the name of
the operation and stops with a non-zero exit code. cir2c does not ignore the
operation. The output goes to a verification tool. A missing operation gives an
incorrect result, and an incorrect result is worse than no result.

### 7.4 The sequence of the module output

C needs a declaration before a use. The C++ constructions that cir2c models have
circular references. A vtable refers to functions. Those functions use the record
type. The methods of the record are in the vtable. `ModuleEmitter` uses this
fixed sequence:

1. **First step.** The `extern` declarations for the vtable symbols and the VTT
   symbols.
2. **The globals.** The global variables. A global whose initializer takes the
   address of a different symbol moves to step 4.
3. **The function declarations.** A prototype for each function. Thus a body can
   call any function.
4. **The remaining globals.** First the vtable arrays, then the VTTs. Their
   functions now have declarations.
5. **The function bodies.** cir2c keeps the bodies in a buffer. Thus the
   `__VERIFIER_nondet_*` declarations that it finds during this step can go to
   the top of the file.

### 7.5 The C models of the C++ constructions

**Virtual dispatch.** cir2c writes the vtables as `void*` arrays. A virtual call
becomes a call to a wrapper function:

```c
/* The vtable pointer of obj is at offset 0. The function is vtable[slot]. */
int __VERIFIER_virtual_call_i_pv(void* __obj, int __slot) {
  void* __fn = ((void**)*(void**)__obj)[__slot];
  return ((int(*)(void*))__fn)(__obj);
}
```

There is one wrapper for each different signature. Each wrapper has one
definition in each output file. A tool that models indirect calls exactly gets
exact dispatch. A different tool sees a call through a pointer from a real
array.

**Exceptions.** There is no `setjmp`, no `longjmp` and no unwinder. Four global
variables hold the exception state:

```c
static void       *__cir_exc_ptr;      /* The exception object */
static const void *__cir_exc_type;     /* The address of the RTTI tag */
static unsigned    __cir_exc_type_id;  /* The numeric tag */
static int         __cir_exc_active;   /* 1 while an exception is active */
```

A `throw` statement sets these variables and returns. After each call that can
throw, cir2c writes a test of `__cir_exc_active`. Each `catch` clause becomes a
comparison with the RTTI tag. Each `typeinfo` symbol becomes a
`static const char __cir_eh_type_<sym>[]` array. Thus the address of the array is
a unique tag.

**Static initialization.** C has no `.init_array` section. cir2c collects the
global constructors and calls them at the start of `main` in the correct
sequence. cir2c writes the destructor calls before each `return` in `main`.

**Names.** cir2c demangles the C++ symbols and makes safe C identifiers. The
`ns::f(int)` symbol becomes `ns__f_int`. If two different symbols make the same
name, cir2c adds a suffix that a person can read. Plain C names do not change.
cir2c uses the demangler only for symbols with the `_Z` prefix.

`operator new` and `operator delete` have no body in CIR. cir2c writes small
functions that call `malloc` and `free`.

**Trivial assignment.** CIR marks a trivial default copy operator or move
operator with `func_info<#cir.cxx_assign<…, trivial true>>`. The body is almost
empty. The mark holds the "copy the object" behavior. cir2c finds the mark and
writes `*dst = *src`. Without this rule, each such assignment does nothing.

### 7.6 Add a CIR operation

1. Make the failure again:
   ```bash
   ./run-cir2c.sh --mlir /tmp/case.mlir case.cpp /tmp/case.c
   ```
   cir2c prints the name of the unknown operation.
2. Find the definition of the operation in the
   `clang/include/clang/CIR/Dialect/IR/CIROps.td` file of the pinned LLVM. Read
   the operands, the results, the attributes and the assembly format.
3. Add the handler to the correct group in `src/handlers/`. Use the
   `registerTypedHandler<cir::TheOp>` function.
4. Add a small test program to `test/integration/input/`. Use the name
   `test_<subject>.c` or `test_<subject>.cpp`. The test runner finds it
   automatically.
5. Run `ctest --test-dir build --output-on-failure`.

If you cannot translate the operation correctly, this is an acceptable result.
Add the operation to the `BLOCKLIST_OPS` list in `test/run_tests.sh` with a
comment. Record it in [8. Limits](#8-limits).

---

## 8. Limits

The output is a model of the input program. This section gives all known
approximations and their direction.

Two words have an exact meaning in this section:

- **Conservative.** The model permits behavior that the real program does not
  have. A proof on the model is also true for the program. A counterexample can
  be false.
- **Unsound.** The model can miss behavior that the real program has. A proof on
  the model is **not** true for the program.

### 8.1 Constructions that cir2c does not translate

For these constructions, cir2c stops with an error. cir2c does not write
incorrect code. The corpus tests ignore these constructions. The
`BLOCKLIST_OPS` list in `test/run_tests.sh` contains them.

| Construction | CIR operation | Cause |
|---|---|---|
| Inline assembly | `cir.asm` | There is no C equivalent. |
| SIMD vectors | `cir.vec.*`, `!cir.vector<…>` | There is no portable C model. |
| Floating-point classification | `cir.is_fp_class` | There is no portable C function. |
| 128-bit integers | `cir.int<s,128>`, `cir.int<u,128>` | Not supported. This also stops cir2c. |
| MSVC SEH | `cir.eh.setjmp`, `cir.eh.longjmp` | This control flow has no model here. |
| Computed goto | `cir.indirect_br` | The GNU `&&label` and `goto *p` extensions have no ISO C equivalent. A sound translation must change the function into a switch statement on a label number. cir2c does not do this yet. |

Clang cannot make CIR for some other constructions. In this condition,
`run-cir2c.sh` returns exit code 2 and Clang gives the cause. Each LLVM release
makes this set smaller.

### 8.2 The standard library

The `--externalize-std` option and the `--no-externalize-std` option control the
most important selection in the tool.

**The `--externalize-std` option (the default) is conservative.**

cir2c replaces the `std::` calls and the library calls with unknown values. For
memory, cir2c writes `__VERIFIER_nondet_memory(p, n)`. cir2c writes this comment
in the output file:

```c
// Over-approximated (may be unsound): library/std:: calls replaced by
//   __VERIFIER_nondet_memory havoc (results are nondeterministic).
```

The model permits more behavior than the program. A proof is true for the
program. A counterexample can be false, because it can use a library result that
the real library does not give.

**WARNING: The `--no-externalize-std` option is UNSOUND for STL containers.**

Some libstdc++ functions have their bodies in compiled `.cc` files and not in
headers or templates. ClangIR sees only a declaration. Thus there is no body in
the CIR, and the output C code has only an `extern` declaration. Examples are
the `_M_hook`, `_M_unhook`, `_M_transfer` and `_M_reverse` functions of
`std::__detail::_List_node_base`. Parts of `std::string` and the locale code
have the same condition.

A verification tool that models an unknown call as an empty function removes
their effects on memory. For `_M_hook`, the list nodes have no connection. The
`begin()` function returns the sentinel. The `front()` function and the `back()`
function read the `_M_size` field of the sentinel as the element. The result is
an incorrect counterexample.

There is no correction in cir2c. The bodies are not in the CIR. A model of these
functions needs specific knowledge of each libstdc++ function. This is not in
the scope of the project. **Do not verify STL container programs with the
`--no-externalize-std` option.**

For this cause, the sequence compiles C++ input with libc++. libc++ keeps
`list`, `map`, `set` and `deque` fully in the headers. Thus ClangIR sees the
bodies. The `run-cir2c.sh` script always gives the `-stdlib=libc++` option.
There is no option to prevent this. The toolchain build installs libc++ with
clang for this cause.

### 8.3 Exceptions

**The catch selection does not use inheritance.** cir2c compares the RTTI tag by
exact pointer equality. Thus a `catch (Base&)` clause does not operate for a
`Derived` exception. In the model, the handler is not accessible. In the
program, the handler operates.

This is conservative. The model examines fewer handler paths. A proof that an
error is not accessible is true. But you cannot show that the program catches an
exception.

The global-variable model has these other effects:

- There is no unwinder. cir2c tests `__cir_exc_active` after each call that can
  throw. If a `throw` statement leaves a construction that cir2c does not model,
  the exception does not continue.
- The `std::terminate` function and the trap code become `abort()`.
- cir2c writes the global destructor calls before each `return` in `main`. They
  do not operate on the `exit()`, `abort()` or `_exit` paths. These paths have no
  one text position.

### 8.4 Flexible array members

A structure can end with an array that has no size:

```c
struct one { int a; int values[]; } hobbit = {5, {1, 2, 3}};
```

CIR gives the member the length 0, but it keeps the correct initial values in
the initializer. cir2c gives the member the length from the initializer. Thus
the values stay correct.

The C compiler cannot initialize an array of length 0. If cir2c used the
declared length, the compiler would discard the values, and the program would
read the memory after the structure.

**Note: The structure becomes larger.** The member is always the last member,
so no other member moves. `sizeof` increases. A calculation that uses `sizeof`
to find a size gives a larger result, never a smaller one.

### 8.5 Structured output and flat output

The default structured output keeps the `for`, `while` and `if` statements and
the scopes. Most verification tools examine this form more efficiently.

The two modes do not accept the same programs. A construction that fails in one
mode can operate in the other mode. This is a limit. But it is also a useful
test. Try the two modes when a translation fails.

### 8.6 Types

- The `long double` type becomes the nearest C type on the host. The
  `x86_fp80`, `bf16` and `fp16` types are in the output. But the bits are not
  always the same as in the source.
- cir2c writes bit fields with explicit get and set operations and not with C
  bit-field syntax. Thus the layout is the layout of CIR and not the layout of
  the C compiler.
- The output uses the target of the pinned toolchain. Do not give the output to a
  tool with a different data model, for example a 32-bit model or a different
  byte sequence.

### 8.7 Other information

- The output is not for a person to read in place of the source. The output is
  not for production use. The tests compile the output only to make sure that it
  is correct C code.
- The tests do not link the output. The C++ library symbols stay unresolved. This
  is correct.
- The `cir2c --version` command prints the commit of the build. The releases
  include the `llvm-version.txt` file. Record the two values with each result.

### 8.8 Report a limit

If you find an approximation that is not in this list, make an issue. Give the
input program, the `--mlir` output and the C output. Soundness problems have a
higher priority than missing functions. Write this in the title.

---

## 9. Tests

### 9.1 The layout

```
test/
├── run_tests.sh                # The test runner for the three suites
├── integration/input/          # Small test programs (the default suite)
├── sources/llvm-test-suite/    # The LLVM SingleSource corpus (2600 programs)
├── benchmark_mapper.py         # The runner for SV-COMP .yml tasks
├── generate-reference-outputs.py
└── results2tex.py              # Converts results.txt to LaTeX macros
```

You must have `bash`, GNU `parallel`, `python3`, a cir2c binary and a ClangIR
toolchain.

### 9.2 Run the tests

```bash
ctest --test-dir build --output-on-failure     # The integration suite
bash test/run_tests.sh                         # The same suite, directly
RUN_LLVM=1 RUN_INTEGRATION=0 bash test/run_tests.sh
JOBS=8 bash test/run_tests.sh --json-out /tmp/summary.json
```

| Variable | Default | Function |
|---|---|---|
| `RUN_INTEGRATION` | `1` | Run the integration tests |
| `RUN_LLVM` | `0` | Run the LLVM SingleSource tests (large and slow) |
| `RUN_ESBMC` | `0` | Run the esbmc-eval tests (the corpus is not in this repository) |
| `JOBS` | `nproc` | The number of parallel jobs |
| `LLVM_EVAL_DIR` | `test/sources/llvm-test-suite/` | The LLVM corpus |
| `ESBMC_EVAL_DIR` | `test/sources/esbmc-eval/` | The esbmc-eval corpus |
| `CIR2C_LLVM_PREFIX` | Found automatically | The toolchain prefix |
| `CIR2C_BUILD_DIR` | `build` | The directory of the cir2c binary |

The `--json-out FILE` option writes a summary for a program to read. CI uses it
to make the results table. The `--externalize-std` option and the
`--no-externalize-std` option select the standard library behavior.

**Note: A run with no tests is an error.** The runner prints `No tests ran` and
returns a non-zero exit code. CI counts an empty suite as a problem. Thus an
incorrect environment cannot show a good result.

### 9.3 Suite 1: the integration tests

The runner processes each `.c` file and each `.cpp` file in
`integration/input/`. A test is successful when the output C code compiles with
the `clang -fsyntax-only` command.

Add your tests to this suite. Put a small test program in `integration/input/`
with the name `test_<subject>.<c|cpp>`. The runner finds it automatically. No
registration is necessary.

The tests examine arithmetic, casts, comparisons, control flow, arrays, records,
globals, floating-point numbers, pointers, integer promotion and bit operations.
The C++ tests examine exceptions, RAII scopes, and virtual dispatch with member
pointers and multiple inheritance.

The runner writes the logs and the output C code to `integration/output/`. Git
ignores this directory.

### 9.4 Suite 2: the LLVM SingleSource tests

This suite uses the `sources/llvm-test-suite/` corpus. For each source with a
`.reference_output` file, the runner translates it, compiles it, links it, runs
it and compares the output and the exit code.

If a difference also occurs with the `clang -fclangir` command, the cause is in
ClangIR. The runner ignores such a test. cir2c cannot be more correct than its
input.

The `generate-reference-outputs.py` script makes the missing `.reference_output`
files. The script compiles and runs each program with the system compiler.

### 9.5 Suite 3: the esbmc-eval tests

This is a C++ corpus with no reference outputs. A test is successful when the
output C code compiles. The corpus is not in this repository. Set the
`ESBMC_EVAL_DIR` variable to your checkout.

### 9.6 The blocklist

The corpus suites ignore a test when its CIR contains an operation that cir2c
does not translate. The counts thus show real limits and not intentional limits.

The `BLOCKLIST_OPS` list is at the top of `run_tests.sh`. Each entry has a
comment with the cause. The runner compares each entry as a fixed string with
the `.mlir` text. Keep this list and
[8.1 Constructions that cir2c does not translate](#81-constructions-that-cir2c-does-not-translate)
in agreement.

### 9.7 The result categories

| Category | Meaning |
|---|---|
| Passed | The C code compiled, linked, ran and gave the correct result. |
| Compiled, not run | The C code is correct, but the linker failed. This is usual for C++ library symbols. |
| Output mismatch | The program ran and gave an incorrect result. **This is an error.** |
| Mapper failed | cir2c made no C code. |
| Compile failed | cir2c made incorrect C code. **This is an error.** |
| Skipped | A blocklisted operation, a Clang failure, or a known ClangIR difference. |
| Timed out | The test used more than 60 seconds. |

Always examine the "Output mismatch" category and the "Compile failed" category.

### 9.8 The benchmark runner

For SV-COMP tasks in `.yml` files:

```bash
python3 test/benchmark_mapper.py <input-dir> [set-file-or-glob ...]
```

| Option | Function |
|---|---|
| `-c`, `--c-mode` | Use C mode (`clang -x c`) and not the default C++ mode |
| `-f`, `--flatten` | Do the `cir-opt -cir-flatten-cfg` step first |
| `-o DIR`, `--output-dir DIR` | Keep the `.mlir` file and the `output.c` file for each task |

The script writes the results to `output.txt` in the current directory.

---

## 10. Contribute

### 10.1 The work sequence

The `main` branch is protected. Make a branch and open a pull request. CI must be
successful. A person must approve the pull request.

1. Make a branch from `main`.
2. Make the change and add a test.
3. Run the integration suite.
4. Open a pull request. CI builds cir2c, runs the test configurations, and
   writes a results table as a comment.

The results table is the review record. It shows skipped tests, and this is
correct. The important condition is that the "mapper failed", "compile failed"
and "output mismatch" counts do not increase. If a count increases, give the
cause in the pull request.

### 10.2 The commit rules

**Each commit message is one line.** Do not write a body. Do not write a list.
Do not write a trailer of any type. This includes the `Co-Authored-By:` trailer.
Do not write the name of an AI tool.

Use the present tense. Make one logical change in each commit. Give the issue
number in the subject, for example `Fix #7: …`.

```
Correct:   Emit real assignment for trivial defaulted operator=
Incorrect: Emit real assignment for trivial defaulted operator=
           <empty line>
           CIR marks these with func_info<#cir.cxx_assign<…, trivial true>> …
```

If you cannot describe a change in one line, divide it into more commits.

An explanation that is too long for the subject goes into a code comment, into
this README, or into the pull request.

**Each commit has the name of a person.** The person who is responsible for the
change is the author. An AI tool that helps you is not the author. This rule
applies to the author field and to the committer field.

```bash
git config user.name "Your Name"
git config user.email you@example.com
```

CI examines the two rules for each pull request. Refer to
`.github/workflows/commit-messages.yml`.

### 10.3 The code rules

**Correctness has a higher priority than coverage.** The output of cir2c goes to
tools that make conclusions from it. An operation with an incorrect translation
is worse than an operation that stops the program. Do not write C code for an
operation that you do not understand. Stop instead. If a translation is an
approximation, record it in [8. Limits](#8-limits). Write a comment in the
output C code when this is important.

**Comments give the cause and not the operation.** The code has many comments in
the positions where a reader can think that there is an error. Examples are the
trivial-assignment rule, the sequence of the module output, and the exception
model. Do the same. If a subsequent reader can "correct" your code and cause an
error, write a comment.

**Style.** Use the style of the adjacent code: two spaces for each indent level,
`camelCase` for local variables and methods, `PascalCase` for types, and a final
underscore for private members. There is no automatic formatter. Do not change
the format of unrelated lines.

**License headers.** Each source file has the Apache 2.0 header. CI examines
this. To add the missing headers:

```bash
python3 .github/scripts/add-license-header.py .github/scripts/license-header-apache .
git restore test/sources   # The LLVM corpus keeps its own header
```

### 10.4 Tests for a change

| Task | Command |
|---|---|
| The integration suite | `ctest --test-dir build --output-on-failure` |
| The LLVM SingleSource suite | `RUN_LLVM=1 RUN_INTEGRATION=0 bash test/run_tests.sh` |
| One program, with the CIR | `./run-cir2c.sh --mlir /tmp/c.mlir in.cpp /tmp/c.c` |

Each change in behavior needs a test in `test/integration/input/`. Each
correction needs a test that fails before the correction and is successful after
it.

### 10.5 Report a problem

Give the input program, the `--mlir` output, the C output and the result of the
`cir2c --version` command. Soundness problems have a higher priority. Write this
in the title.

---

## 11. License

Apache License 2.0. Refer to the [LICENSE](LICENSE) file. The
[CONTRIBUTORS.md](CONTRIBUTORS.md) file gives the list of contributors.

The binary releases include LLVM and Clang binaries. The Apache License 2.0 with
LLVM Exceptions applies to them. The releases include this license as the
`LICENSE.LLVM` file.

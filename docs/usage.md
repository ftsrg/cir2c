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

# Usage

## `run-cir2c.sh` — the whole pipeline

`run-cir2c.sh` is the recommended entry point. It drives all four stages —
`clang -emit-cir` → preprocess → optional flatten → `cir2c` — so you hand it a
source file and get C back.

```bash
./run-cir2c.sh input.c   output.c     # C input
./run-cir2c.sh input.cpp output.c     # C++ input (inferred from the extension)
```

The output directory is created if it does not exist.

### Options

```
run-cir2c.sh [OPTIONS] <input-file> <output.c>
```

| Option | Purpose |
|---|---|
| `--lang c\|c++` | Force the input language instead of inferring it from the extension (`.cpp`/`.cxx`/`.cc`/`.C` → C++, anything else → C). |
| `--std STD` | Language standard. Default `c23` for C, `c++23` for C++. |
| `--flatten` | Run `cir-opt -cir-flatten-cfg` before `cir2c`, producing flat (goto-based) C instead of structured C. |
| `--mlir FILE` | Also save the intermediate CIR to `FILE`. The single most useful debugging flag. |
| `--flat-mlir FILE` | Save the flattened CIR to `FILE`; implies `--flatten`. |
| `--include DIR` | Add `-I DIR` to the CIR-generation step. Repeatable. |
| `--externalize-std` | *(default)* Replace `std::` and library calls with nondeterministic havoc. |
| `--no-externalize-std` | Keep the real `std::` bodies. Read [limitations.md](limitations.md#externalizing-the-standard-library) first — this is unsound for STL container code. |

### Exit codes

| Code | Meaning |
|---|---|
| 0 | Success |
| 2 | `clang` failed — bad source, or a construct ClangIR cannot lower yet |
| 3 | `cir-opt` failed during `--flatten` |
| 4 | `cir2c` failed — usually an unsupported CIR operation |

Exit 2 is worth reading carefully: it means Clang could not produce CIR at all,
which is upstream ClangIR's limitation rather than cir2c's.

### Structured vs. flat output

By default cir2c works on **non-flat (structured) CIR**: `cir.for`, `cir.while`,
`cir.if`, `cir.scope` and `cir.try` are still regions, and the emitted C has
real `for`/`while`/`if` statements and lexical scopes. This is the mode to
prefer — verifiers reason better about structured control flow, and the output
is legible.

`--flatten` runs `-cir-flatten-cfg` first, lowering all of that to basic blocks
and branches. The emitted C is then labels and `goto`s. Use it when a construct
does not map in structured form, or when the consumer prefers a CFG.

### Where the tools come from

`run-cir2c.sh` looks for clang, clang++ and cir-opt under, in order:
`$CIR2C_LLVM_PREFIX`, `<repo>/llvm-install`, `/opt/cir`, then the `PATH`. It
looks for the `cir2c` binary at `$CIR2C_BIN`, else
`${CIR2C_BUILD_DIR:-<repo>/build}/cir2c`.

```bash
export CIR2C_LLVM_PREFIX=/opt/cir
export CIR2C_BUILD_DIR=/path/to/build
./run-cir2c.sh input.cpp output.c
```

If nothing usable is found the script says what it looked for and how to fix it.

## `cir2c` — the binary

If you already have a CIR module, call the binary directly. You are then
responsible for generating and preprocessing the CIR yourself.

```
cir2c [--monitor-json <trace.json>] [--[no-]externalize-std] <input.mlir> <output.c>
cir2c --version
```

| Argument | Meaning |
|---|---|
| `<input.mlir>` | A CIR module, from `clang -S -emit-cir` and optionally `cir-opt -cir-flatten-cfg` |
| `<output.c>` | Path for the generated C |
| `--version` | Print the version (the git commit cir2c was built from) and exit |
| `--monitor-json FILE` | Write a JSON trace mapping each CIR operation to the output C lines it produced |
| `--[no-]externalize-std` | As above; the default is `--externalize-std` |

Exit codes are `0` success, `2` bad arguments, `3`/`5` input/output file could
not be opened, `4` the MLIR failed to parse, `6` mapping failed.

### Traceability output

`--monitor-json` records, per CIR operation, the operation text, the C it
produced, and the line ranges on both sides. It is meant for building the kind
of tooling that shows a source construct and its translation side by side, and
for auditing a translation when a verifier reports something surprising.

Computing the line mapping is skipped entirely unless the flag is given, since
its cost scales with (operation count × output line count).

## Working on a failing case

1. Save the intermediate CIR:
   ```bash
   ./run-cir2c.sh --mlir /tmp/case.mlir case.cpp /tmp/case.c
   ```
2. If the run failed with exit 2, the problem is upstream: Clang could not emit
   CIR for that source. Read its diagnostics.
3. If it failed with exit 4, the CIR is in `/tmp/case.mlir` and cir2c could not
   map some operation in it. cir2c names the operation on stderr; grep for it in
   the `.mlir`, and see `src/handlers/` for where a handler would go.
4. If the C came out but is wrong, re-run the binary directly on the saved
   `.mlir` — that isolates cir2c from the clang and cir-opt stages, and makes
   the edit-build-run loop fast.
5. `--flatten` sometimes succeeds where structured emission does not, and vice
   versa; comparing the two is a quick way to localise the problem.

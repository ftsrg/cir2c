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

# Test suite

```
test/
├── run_tests.sh                # the runner for all three suites
├── integration/input/          # small, targeted reproducers (the default suite)
├── sources/llvm-test-suite/    # vendored LLVM SingleSource corpus (~2600 programs)
├── benchmark_mapper.py         # SV-COMP-style .yml benchmark runner
├── generate-reference-outputs.py
└── results2tex.py              # results.txt → LaTeX macros, for papers
```

Requires `bash`, GNU `parallel`, `python3`, and a built `cir2c` plus a ClangIR
toolchain — see [../docs/building.md](../docs/building.md).

## Running

The short version, from a configured build directory:

```bash
ctest --test-dir build --output-on-failure
```

That runs the integration suite. To drive the runner directly:

```bash
bash test/run_tests.sh                       # integration only (the default)
RUN_LLVM=1 RUN_INTEGRATION=0 bash test/run_tests.sh
JOBS=8 bash test/run_tests.sh --json-out /tmp/summary.json
```

| Variable | Default | Meaning |
|---|---|---|
| `RUN_INTEGRATION` | `1` | Integration tests |
| `RUN_LLVM` | `0` | LLVM SingleSource end-to-end tests (large, slow) |
| `RUN_ESBMC` | `0` | esbmc-eval coverage tests (large, slow; corpus not vendored) |
| `JOBS` | `nproc` | Parallelism |
| `LLVM_EVAL_DIR` | `test/sources/llvm-test-suite/` | LLVM corpus location |
| `ESBMC_EVAL_DIR` | `test/sources/esbmc-eval/` | esbmc-eval corpus location |
| `CIR2C_LLVM_PREFIX` | probed | ClangIR toolchain prefix |
| `CIR2C_BUILD_DIR` | `build` | Where to find the `cir2c` binary |

Flags: `--json-out FILE` writes a machine-readable summary (CI uses it to build
the results table); `--externalize-std` / `--no-externalize-std` select the
standard-library treatment (see [../docs/limitations.md](../docs/limitations.md#externalizing-the-standard-library)).

## Suite 1 — integration (`RUN_INTEGRATION=1`)

Every `.c` and `.cpp` file under `integration/input/`, run through the full
pipeline; the test passes when the generated C compiles with `clang -fsyntax-only`.

This is the suite to extend. Drop a minimal reproducer into
`integration/input/`, named `test_<area>.<c|cpp>`, and it is picked up
automatically — no registration anywhere. The current inputs cover arithmetic,
casts, comparisons, control flow, arrays, structs, globals, floats, pointers,
integer promotion, bit operations; C++ exception handling and RAII scopes;
and virtual dispatch, including member pointers and multiple inheritance.

Per-test logs and generated C land in `integration/output/` (gitignored).

## Suite 2 — LLVM SingleSource (`RUN_LLVM=1`)

The vendored `sources/llvm-test-suite/` corpus. For each source with a
`.reference_output` sibling: translate, compile, link, run, and compare stdout
and exit code against the reference. Sources without a reference output are not
run.

Output mismatches that reproduce with `clang -fclangir` directly are attributed
to upstream ClangIR and counted as skipped rather than failed — cir2c cannot be
more correct than the CIR it is handed.

`generate-reference-outputs.py` fills in missing `.reference_output` files by
compiling and running each program with the system gcc/g++.

## Suite 3 — esbmc-eval (`RUN_ESBMC=1`)

A C++ coverage corpus with no reference outputs; the pass condition is only that
the generated C **compiles**. The corpus is not vendored here — point
`ESBMC_EVAL_DIR` at a checkout of it.

## The blocklist

The corpus suites skip tests whose generated CIR contains an operation that is
knowingly out of scope — inline assembly, SIMD vectors, `__int128`, MSVC SEH.
These count as skipped rather than failed, so the numbers reflect real coverage
gaps rather than deliberate ones.

The list is the `BLOCKLIST_OPS` array at the top of `run_tests.sh`, one entry
per line with a comment saying why. Add and remove entries freely; each entry is
matched as a fixed substring against the `.mlir` text. Keep it in sync with
[../docs/limitations.md](../docs/limitations.md#not-mapped-at-all).

## Result categories

The runner distinguishes failure modes, because they mean different things:

| Category | Meaning |
|---|---|
| **Passed** | Generated C compiled, linked, ran, and matched the reference |
| **Compiled, not run** | Valid C, but could not be linked (expected for unresolved C++-stdlib references) |
| **Output mismatch** | Ran, but produced the wrong answer — a real bug |
| **Mapper failed** | The pipeline could not produce C |
| **Compile failed** | cir2c produced invalid C — a real bug |
| **Skipped** | Blocklisted op, a CIR-generation failure upstream, or a known ClangIR divergence |
| **Timed out** | Exceeded the 60 s per-test budget |

"Mismatch" and "compile failed" are the two that always deserve investigation.

## Benchmark runner

For SV-COMP-style benchmarks expressed as `.yml` task files:

```bash
python3 test/benchmark_mapper.py <input-dir> [set-file-or-glob ...]
```

| Flag | Meaning |
|---|---|
| `-c` / `--c-mode` | C mode (`clang -x c`) instead of the default C++ mode |
| `-f` / `--flatten` | Run `cir-opt -cir-flatten-cfg` before cir2c |
| `-o DIR` / `--output-dir DIR` | Keep per-case `.mlir` and `output.c` |

Results are written to `output.txt` in the working directory.

## Debugging a failure

1. Find the per-test log under `integration/output/`, `llvm-eval/output/` or
   `esbmc-eval/output/`.
2. Re-run the single case with the intermediate CIR saved:
   ```bash
   ./run-cir2c.sh --mlir /tmp/case.mlir path/to/case.cpp /tmp/case.c
   ```
3. Iterate on `cir2c /tmp/case.mlir /tmp/case.c` alone — that takes clang out of
   the loop entirely.

See [../docs/usage.md](../docs/usage.md#working-on-a-failing-case) for the
longer version.

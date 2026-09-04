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

# Limitations

The C that cir2c emits is a **model** of the input program, not a translation of
it. Some parts of C++ have no C equivalent that a verifier can reason about, and
in those places cir2c approximates. This page lists every approximation we know
about, and says in which direction each one errs.

Two words are used precisely throughout:

- **Conservative** — the model admits behaviours the real program does not.
  A *proof* on the model still holds for the program; a *counterexample* may be
  spurious.
- **Unsound** — the model can miss behaviours the real program has. A proof on
  the model does **not** transfer. These are called out in capitals, both here
  and in a comment in the generated C.

## Not mapped at all

These constructs cause cir2c to fail rather than emit something wrong. In the
corpus test suites they are skipped by an explicit blocklist in
`test/run_tests.sh`.

| Construct | CIR | Why |
|---|---|---|
| Inline assembly | `cir.asm` | No C-level meaning to give it |
| SIMD vectors | `cir.vec.*`, `!cir.vector<…>` | No portable C model |
| FP classification | `cir.is_fp_class` | No portable C builtin |
| 128-bit integers | `cir.int<s,128>`, `cir.int<u,128>` | Not supported, and currently crashes the mapper |
| MSVC SEH / `__builtin_setjmp` | `cir.eh.setjmp`, `cir.eh.longjmp` | Non-local control flow that is not modellable in this scheme |

An unhandled operation is always a hard failure — cir2c never silently drops
one. For output that feeds a verifier, dropping an operation would produce a
confident wrong answer.

Separately, anything **upstream ClangIR** cannot lower never reaches cir2c at
all: `run-cir2c.sh` exits 2 and Clang's own diagnostic explains why. That set
shrinks with every LLVM release.

## Externalizing the standard library

This is the most consequential choice in the tool, and it is a genuine
trade-off. It is controlled by `--externalize-std` (default) and
`--no-externalize-std`.

### `--externalize-std` (default): conservative

`std::` and library calls are replaced by nondeterministic havoc — the result
becomes an unconstrained value, and `__VERIFIER_nondet_memory(p, n)` is used
where memory is written. The generated file says so:

```c
// Over-approximated (may be unsound): library/std:: calls replaced by
//   __VERIFIER_nondet_memory havoc (results are nondeterministic).
```

This over-approximates: the model allows *more* behaviour than the program.
A proof carries over; a counterexample may be spurious, because it may depend
on a library return value the real library would never produce.

It is unsound in one specific direction: for code whose correctness depends on
what the library call actually returns or writes, an "everything is possible"
result is not a faithful model of a library with a contract.

### `--no-externalize-std`: **UNSOUND for STL containers**

Keeping the real `std::` bodies sounds strictly better, and for most code it is.
It is not for containers, and the reason is structural rather than a bug in
cir2c.

A handful of libstdc++ functions are defined **out of line**, in compiled `.cc`
units rather than in headers, inline functions or templates. ClangIR therefore
only ever sees a declaration for them — there is no body in the CIR to emit, so
the generated C has a bare `extern`. Examples: `std::__detail::_List_node_base`'s
`_M_hook` / `_M_unhook` / `_M_transfer` / `_M_reverse` (from `list.cc`), and
parts of `std::string` and the locale machinery.

A verifier that models an undefined call as a no-op then **silently drops their
pointer side effects**. For `_M_hook` that means list nodes are never linked:
`begin()` returns the sentinel header, and `front()`/`back()` read the header's
`_M_size` field as if it were the element — producing a confident, wrong
counterexample.

There is no general fix in the mapper: the bodies genuinely do not exist in the
CIR. Modelling them would mean hard-coding per-function knowledge of libstdc++
internals, which was considered and rejected as too fragile. **Verifying STL
container programs under `--no-externalize-std` is out of scope.**

This is why the pipeline compiles C++ against **libc++**, not libstdc++:
libc++ keeps `list`/`map`/`set`/`deque` fully header-templated, so ClangIR sees
real bodies. `run-cir2c.sh` passes `-stdlib=libc++` unconditionally, with no
flag to opt out, and the toolchain build installs libc++ alongside clang for
exactly this reason.

## Exception handling

**Catch matching ignores inheritance.** Dispatch compares the RTTI tag by exact
pointer equality, so `catch (Base&)` does **not** fire for a thrown `Derived`.
The handler is unreachable in the model where it would have run in the program.

This direction is conservative: the model explores strictly fewer handler paths.
A proof that no error is reachable still holds. What you can lose is the ability
to show that an exception *is* caught.

Other consequences of the global-state EH model:

- There is no stack unwinder. Propagation is a check of `__cir_exc_active` after
  each call that can throw, so a `throw` that escapes a construct cir2c does not
  model is not propagated.
- `std::terminate` and the trap paths lower to `abort()`.
- Global destructors are injected before each `return` from `main`; they do
  **not** run on `exit()`, `abort()` or `_exit` paths, which have no single
  textual return point.

## Structured versus flat output

Default (structured) emission preserves `for`/`while`/`if`/scopes, which is what
most verifiers reason about best. `--flatten` lowers everything to basic blocks
and emits labels and `goto`s.

The two modes do not always accept the same programs: a construct that fails in
one sometimes succeeds in the other. That is a limitation, not a feature — but
it is a useful diagnostic, so try both when something fails to map.

## Types

- `long double` maps to the nearest C type available on the host; exotic
  floating-point widths (`x86_fp80`, `bf16`, `fp16`) are emitted but not
  guaranteed bit-identical to the source semantics on all targets.
- Bit-fields are emitted with explicit get/set helper sequences rather than C
  bit-field syntax, so the layout is the one CIR computed, not the one the
  consuming C compiler would pick.
- The generated C encodes the layout decisions of the *pinned* toolchain's
  target. Feeding the output to a verifier assuming a different data model
  (32-bit, different endianness) is not supported.

## Not a limitation, but worth knowing

- The output is not intended to be read by humans as a substitute for the
  source, nor to be compiled and shipped. It is compiled in the test suites only
  to check that it is valid C.
- Linking the generated C is *not* required to pass the corpus suites; the
  C++-stdlib references it leaves unresolved are expected.
- `cir2c --version` prints the commit it was built from, and binary releases
  ship the `llvm-version.txt` they were built against. Both are worth recording
  next to any experimental result.

## Reporting one

If you hit an approximation not listed here — especially one that could turn a
proof into a wrong answer — please open an issue with the input program, the
`--mlir` dump, and the generated C. Soundness bugs take priority over coverage
gaps.

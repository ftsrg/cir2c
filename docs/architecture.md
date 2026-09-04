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

# Architecture

## The pipeline

```
  input.cpp
     │  clang -x c++ -std=c++23 -S -emit-cir  (-stdlib=libc++)
     ▼
  CIR module (MLIR text)
     │  qualifier preprocessing
     ▼
  CIR module            ──optional──▶  cir-opt -cir-flatten-cfg  ──▶  flat CIR
     │                                                                    │
     ▼                                                                    ▼
  cir2c: structured emission                              cir2c: goto-based emission
     │                                                                    │
     └──────────────────────────────▶  output.c  ◀────────────────────────┘
```

Each stage is a separate process, and every intermediate is text you can read.
That is deliberate: when a translation looks wrong, the CIR that produced it can
be dumped (`--mlir`), edited, and re-fed to `cir2c` alone.

## Why CIR rather than LLVM IR

A verifier that only parses C needs C++ decomposed for it, but not *destroyed*.
Lowering to LLVM IR does too much: loops become branch soup, records become
byte offsets, and exception handling becomes landing pads whose relationship to
the original `try` is gone.

CIR sits at the level in between. Templates are instantiated, overloads
resolved, constructors and destructors made explicit, RAII cleanups
materialised — all the C++-only machinery is gone — but `cir.for`, `cir.while`,
`cir.if`, `cir.scope`, `cir.try` and `cir.vtable.*` are still there as
structure. That is why the emitted C contains real loops and scopes rather than
a label maze.

## Inside cir2c

cir2c does not build CIR; it *reads* it. `main.cpp` registers the CIR dialect
with an MLIR context and parses the input module with MLIR's generic assembly
parser. Everything after that is a walk over `mlir::Operation`s.

| Component | Responsibility |
|---|---|
| `Mapper` (`Mapper.h/.cpp`) | Central state and dispatch: the handler table, SSA-value → C-expression bindings, scope and loop stacks, symbol naming, the set of intrinsics that need declaring |
| `ModuleEmitter.cpp` | Module-level emission: the multi-pass ordering of globals, function declarations, vtables and function bodies |
| `TypeMapper` | CIR types → C types (`!cir.int<s, 32>` → `int`, `!cir.ptr<T>` → `T*`, records → `struct`) |
| `ConstantEmitter` | CIR constant attributes → C initialisers, including nested arrays and records |
| `Traceability` | Per-operation MLIR ↔ C line mapping behind `--monitor-json` |
| `handlers/` | One file per family of CIR operations |

### Handlers

Emission for a given operation lives in a handler keyed by operation name
(`cir.alloca`, `cir.for`, `cir.throw`, …). Handler families are grouped by
subject — `ArithmeticHandlers`, `ControlFlowHandlers`, `ExceptionHandlers`,
`VTableHandlers`, `MemoryHandlers`, `RecordHandlers`, `CallHandlers`,
`CastHandlers`, `ComplexHandlers`, `FloatMathHandlers`, `BuiltinHandlers`,
`ConstantHandlers`, `GlobalHandlers`, `VariadicHandlers` — and each family
self-registers at static-init time via `REGISTER_HANDLER_MODULE`, so adding a
family means adding a `.cpp` to `CMakeLists.txt` and nothing else.

Handlers are typed. `Mapper::registerTypedHandler<cir::LoadOp>(...)` derives the
operation name from the generated op class and hands the handler an already
`dyn_cast` operation, so handler bodies deal in `cir::LoadOp`, not raw
`Operation*` plus attribute lookups.

**An unhandled operation is a hard failure, not a silent skip.** cir2c reports
the operation and exits non-zero rather than emitting C that quietly drops it.
For a tool whose output feeds a verifier, a missing operation is a wrong answer,
and a wrong answer is worse than no answer.

### Module emission order

C needs things declared before use, and the C++ constructs cir2c models have
circular references (a vtable names functions; those functions take the record
type; the record's methods appear in the vtable). `ModuleEmitter` resolves this
with a fixed pass order:

1. **Pre-pass** — `extern` declarations for vtable and VTT symbols.
2. **Globals** — global variables. Ones whose initialiser takes the address of
   another symbol are deferred to step 4.
3. **Function declarations** — prototypes for everything, so bodies can call in
   any order.
4. **Late globals** — vtable arrays first, then VTTs, now that the functions
   they name are declared.
5. **Function bodies** — buffered rather than written straight out, so the
   `__VERIFIER_nondet_*` externs discovered while emitting them can still be
   placed at the top of the file.

## How C++ features are modelled in C

### Virtual dispatch

Vtables are emitted as real `void*` arrays. A virtual call becomes a call to a
generated wrapper:

```c
// obj's vtable pointer is at offset 0; the function is vtable[slot]
int __VERIFIER_virtual_call_i_pv(void* __obj, int __slot) {
  void* __fn = ((void**)*(void**)__obj)[__slot];
  return ((int(*)(void*))__fn)(__obj);
}
```

One wrapper per distinct signature, defined exactly once per output file. A
verifier that models indirect calls precisely gets exact dispatch; one that does
not still sees a call through a pointer loaded from a real, initialised array.

### Exceptions

There is no `setjmp`/`longjmp` and no unwinder. Exception state is four file-local
globals:

```c
static void       *__cir_exc_ptr;      /* the in-flight exception object */
static const void *__cir_exc_type;     /* RTTI tag address */
static unsigned    __cir_exc_type_id;  /* numeric tag */
static int         __cir_exc_active;   /* 1 while an exception is in flight */
```

`throw` fills them and returns; each call that can throw is followed by a check
of `__cir_exc_active`; `catch` clauses become explicit comparisons against the
RTTI tag. Each `typeinfo` symbol becomes a `static const char
__cir_eh_type_<sym>[]` so its *address* is a unique comparable tag.

Dispatch is exact pointer equality on that tag, which means `catch (Base&)` does
not fire for a `Derived` exception — see [limitations.md](limitations.md).

### Static initialisation

C has no `.init_array`. Global constructors are collected during emission and
called explicitly at the top of `main`, in loader order; the matching destructor
calls are injected before each `return` from `main`.

### Naming

C++ symbols are demangled and sanitised into C identifiers — `ns::f(int)`
becomes `ns__f_int`. Distinct symbols that sanitise to the same name get a
readable unique suffix rather than a hash. Plain C names are passed through
untouched (the demangler is only invoked on `_Z`-prefixed symbols).

`operator new` and `operator delete` are declared but bodiless in CIR, so cir2c
emits inline stubs wrapping `malloc`/`free` instead of leaving them undefined.

### Trivial defaulted assignment

CIR marks a trivial defaulted copy/move-assignment operator with
`func_info<#cir.cxx_assign<…, trivial true>>` and gives it an essentially empty
body — the "bitwise-copy the object" semantics live in the marker, on the
assumption that the eventual lowering materialises the copy. cir2c detects the
marker and emits `*dst = *src` rather than transcribing the empty body, which
would silently turn every such assignment into a no-op.

## Adding support for a new CIR operation

1. Reproduce it: `./run-cir2c.sh --mlir /tmp/case.mlir case.cpp /tmp/case.c`.
   cir2c names the unhandled operation on stderr.
2. Find its definition in the pinned LLVM's
   `clang/include/clang/CIR/Dialect/IR/CIROps.td` — operands, results,
   attributes, assembly format.
3. Add the handler to the family it belongs to in `src/handlers/`, using
   `registerTypedHandler<cir::TheOp>`.
4. Add a minimal reproducer under `test/integration/input/`, named
   `test_<area>.c` or `.cpp`. It is picked up automatically.
5. Run `ctest --test-dir build --output-on-failure`.

If the operation cannot be mapped soundly, that is a legitimate outcome: add it
to the blocklist in `test/run_tests.sh` with a comment saying why, so the corpus
suites report it as skipped rather than failed, and record it in
[limitations.md](limitations.md).

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

# Contributing

## Getting set up

Follow [docs/building.md](docs/building.md). The fastest route is the prebuilt
toolchain image; building LLVM yourself takes hours and is only worth it if you
also want to hack on ClangIR.

```bash
docker run --rm -it -v "$PWD:/src" -w /src ghcr.io/ftsrg/cir2c-llvm:latest bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Read [docs/architecture.md](docs/architecture.md) before your first change —
particularly the module emission order, which is the part that surprises people.

## Workflow

`main` is protected: work on a branch and open a pull request. CI must be green
and the pull request needs a review before it can merge.

1. Branch from `main`.
2. Make the change, with a test.
3. Run the integration suite locally.
4. Open a pull request. CI builds cir2c, runs all four test configurations, and
   posts a results table as a comment.

The results table is the review artifact: it is expected to show skips, and what
matters is that **mapper failures, compile failures and output mismatches do not
increase**. If a number moves in the wrong direction, say why in the pull
request rather than leaving the reviewer to work it out.

## Adding support for a CIR operation

The full walkthrough is in
[docs/architecture.md](docs/architecture.md#adding-support-for-a-new-cir-operation).
In short: find the operation's definition in the pinned LLVM's `CIROps.td`, add
a handler to the appropriate family in `src/handlers/` via
`registerTypedHandler<cir::TheOp>`, and drop a reproducer into
`test/integration/input/`.

If an operation cannot be mapped soundly, saying so is a valid outcome: add it
to `BLOCKLIST_OPS` in `test/run_tests.sh` with a comment, and record it in
[docs/limitations.md](docs/limitations.md).

## Conventions

**Soundness beats coverage.** cir2c's output feeds tools that will draw
conclusions from it, so an operation that is silently mistranslated is worse
than one that fails loudly. Never emit C for an operation you do not understand;
fail instead. If a mapping is an approximation, say so in
[docs/limitations.md](docs/limitations.md) and, where it matters, in a comment
in the generated C.

**Comments explain why, not what.** The existing code is unusually
comment-dense in the places where a reader would otherwise assume a bug — the
trivial-assignment special case, the module emission order, the exception model.
Match that: if a future reader might "clean up" your code and break it, leave a
note saying what it protects against.

**Style.** Follow the surrounding code: two-space indent, LLVM-ish naming
(`camelCase` locals and methods, `PascalCase` types, trailing-underscore
private members). No formatter is enforced; do not reformat unrelated lines.

**License headers.** Every source file carries the Apache 2.0 header. CI
enforces this. To add missing ones:

```bash
python3 .github/scripts/add-license-header.py .github/scripts/license-header-apache .
git restore test/sources   # vendored LLVM corpus keeps its own header
```

**Commits — subject line only.** Every commit message in this repository is a
single line. No body, no bullet list, no trailers of any kind — including
`Co-Authored-By:`, and including anything identifying an AI assistant. CI
enforces this on every pull request; `git log --oneline` is the whole log.

Present tense, one logical change each, and reference an issue in the subject
(`Fix #7: …`) when there is one. Rationale that does not fit in a subject line
belongs in the code as a comment, in `docs/`, or in the pull request — those are
where a reader will actually look for it later.

```
Good:  Emit real assignment for trivial defaulted operator=
Bad:   Emit real assignment for trivial defaulted operator=
       <blank line>
       CIR marks these with func_info<#cir.cxx_assign<…, trivial true>> and …
```

If a change genuinely cannot be explained in one line, that is usually a sign it
should be more than one commit.

**Authorship.** Commits are attributed to the person responsible for the
change — never to an assistant that helped write it. If you use an AI tool, the
commit is still yours: set your own identity and leave it set.

```bash
git config user.name "Your Name"
git config user.email you@example.com
```

Enable the repository's hooks once per clone, and both rules are checked before
the commit is made rather than by CI afterwards:

```bash
git config core.hooksPath .githooks
```

## Testing

| What | Command |
|---|---|
| Integration suite | `ctest --test-dir build --output-on-failure` |
| LLVM SingleSource | `RUN_LLVM=1 RUN_INTEGRATION=0 bash test/run_tests.sh` |
| One case, with CIR dumped | `./run-cir2c.sh --mlir /tmp/c.mlir in.cpp /tmp/c.c` |

Every behavioural change needs a test in `test/integration/input/`. A bug fix
needs a reproducer that fails before it and passes after.

See [test/README.md](test/README.md) for the whole picture.

## Updating LLVM

Moving the pinned LLVM release is a specific procedure with its own page:
[docs/llvm-toolchain.md](docs/llvm-toolchain.md#moving-the-pin). Do not bump
`llvm-version.txt` without rebuilding the toolchain image and running the corpus
suites — the failures it causes are usually renamed accessors and changed
assembly syntax, and they need fixing in the same pull request.

## Reporting bugs

Include the input program, the `--mlir` dump, the generated C, and the output of
`cir2c --version`. Soundness bugs — anything that could turn a proof into a
wrong answer — take priority over coverage gaps; say so in the title.

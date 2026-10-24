#!/usr/bin/env bash
#
# Copyright 2025 Budapest University of Technology and Economics
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Tests for cir-merge. Each directory under cases/ holds the inputs of one
# merge (*.c and *.cpp are compiled to CIR with the toolchain's clang, *.cir
# are used as they are; headers are only included) and an `expect` file:
#
#   ok | error                first line: whether the merge must succeed
#   <text>                    (error) a line the diagnostics must contain
#   output-count <n> <text>   (ok) the merged CIR contains <text> n times
#   run <code>                (ok) translate the merged CIR with cir2c,
#                             compile it, run it, expect exit code <code>
#   compile                   (ok) translate the merged CIR with cir2c and
#                             compile the C (without linking it)
#   reference <file>          (ok) like run, but compare the output and the
#                             exit code with an LLVM test-suite reference file
#                             (a path relative to the repository)
#
# A `sources` file lists inputs by repository-relative path instead (used for
# programs of the LLVM corpus). A `// cir-merge-cflags: <flags>` line in a
# source adds compiler flags for that source; a `cflags` file in the case
# directory adds flags for all its sources.
#
# The outcome must not depend on the order of the inputs: a case with up to
# three inputs is merged in every order, a larger one in sorted and in reverse
# order. A successful merge must also parse and verify again with cir-opt.
# After the cases, a few checks cover the command line and run-cir2c.sh.
#
# Usage: test/cir-merge/run_tests.sh [case-name-substring]

set -uo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
CIR2C_REPO_ROOT=$(cd -- "$SCRIPT_DIR/../.." && pwd)
# shellcheck source=scripts/cir-toolchain.sh
source "$CIR2C_REPO_ROOT/scripts/cir-toolchain.sh"
cir_require_toolchain --libcxx --cir-merge || exit 1

FILTER="${1:-}"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

pass=0
fail=0

emit_cir() {
    local src="$1" out="$2"
    local -a extra=()
    local flags
    flags=$(sed -n 's|.*cir-merge-cflags: *||p' "$src" | head -1)
    [[ -f "$CASE_CFLAGS" ]] && flags="$(cat "$CASE_CFLAGS") $flags"
    # shellcheck disable=SC2206 # the flags are meant to be split
    [[ -n "$flags" ]] && extra=($flags)
    case "$src" in
        *.cpp) "$CIR_CLANGXX" -std=c++23 -stdlib=libc++ -nostdinc++ -isystem "$CIR_LIBCXX" \
                   ${extra[@]+"${extra[@]}"} -S -emit-cir "$src" -o "$out" 2>/dev/null ;;
        *.c)   "$CIR_CLANG" -std=c23 ${extra[@]+"${extra[@]}"} \
                   -S -emit-cir "$src" -o "$out" 2>/dev/null ;;
    esac
}

# translate_and_run <merged.cir> <work-dir>: leaves the program's stdout in
# <work-dir>/stdout and its exit code in $run_code; prints a reason and returns
# 1 when the program cannot be built.
translate_and_run() {
    local merged="$1" work="$2"
    if ! "$CIR2C_BIN" --no-externalize-std "$merged" "$work/out.c" >"$work/cir2c.log" 2>&1; then
        echo "cir2c failed:"; sed 's/^/    /' "$work/cir2c.log"; return 1
    fi
    if ! "$CIR_CLANG" -w "$work/out.c" -o "$work/out" -lm 2>"$work/cc.log"; then
        echo "the translated C does not compile:"; sed 's/^/    /' "$work/cc.log"; return 1
    fi
    (cd "$work" && timeout 60 ./out >"$work/stdout" 2>/dev/null)
    run_code=$?
    return 0
}

# permutations <items...>: prints every order of up to three items, one per
# line; for more items only the given order and its reverse.
permutations() {
    local n=$#
    if [[ $n -le 1 ]]; then
        echo "$*"
    elif [[ $n -eq 2 ]]; then
        echo "$1 $2"; echo "$2 $1"
    elif [[ $n -eq 3 ]]; then
        echo "$1 $2 $3"; echo "$1 $3 $2"; echo "$2 $1 $3"
        echo "$2 $3 $1"; echo "$3 $1 $2"; echo "$3 $2 $1"
    else
        echo "$*"
        local -a rev=()
        local i
        for ((i = n; i >= 1; i--)); do rev+=("${!i}"); done
        echo "${rev[*]}"
    fi
}

# check_case <case-dir> <work-dir> <inputs...>; prints a reason and returns 1
# on failure.
check_case() {
    local dir="$1" work="$2"; shift 2
    local expect="$dir/expect"
    local outcome; outcome=$(head -1 "$expect")
    local merged="$work/merged.cir" log="$work/merge.log"
    rm -f "$merged"

    "$CIR_MERGE_BIN" --output "$merged" "$@" >"$log" 2>&1
    local rc=$?

    if [[ "$outcome" == error ]]; then
        if [[ $rc -eq 0 ]]; then
            echo "merge succeeded but was expected to fail"; return 1
        fi
        if [[ -e "$merged" ]]; then
            echo "a failed merge left an output file behind"; return 1
        fi
        local line
        while IFS= read -r line; do
            [[ -z "$line" ]] && continue
            if ! grep -qF -- "$line" "$log"; then
                echo "diagnostics lack: $line"; sed 's/^/    /' "$log"; return 1
            fi
        done < <(tail -n +2 "$expect")
        return 0
    fi

    if [[ $rc -ne 0 ]]; then
        echo "merge failed:"; sed 's/^/    /' "$log"; return 1
    fi
    if ! "$CIR_OPT" "$merged" -o /dev/null 2>"$work/reparse.log"; then
        echo "cir-opt rejects the merged module:"; sed 's/^/    /' "$work/reparse.log"; return 1
    fi

    local directive arg rest
    while read -r directive arg rest; do
        case "$directive" in
            output-count)
                local n; n=$(grep -oF -- "$rest" "$merged" | wc -l)
                if [[ "$n" -ne "$arg" ]]; then
                    echo "merged CIR contains '$rest' $n times, expected $arg"; return 1
                fi ;;
            compile)
                if ! "$CIR2C_BIN" --no-externalize-std "$merged" "$work/out.c" >"$work/cir2c.log" 2>&1; then
                    echo "cir2c failed:"; sed 's/^/    /' "$work/cir2c.log"; return 1
                fi
                if ! "$CIR_CLANG" -w -c "$work/out.c" -o "$work/out.o" 2>"$work/cc.log"; then
                    echo "the translated C does not compile:"; sed 's/^/    /' "$work/cc.log"; return 1
                fi ;;
            run)
                translate_and_run "$merged" "$work" || return 1
                if [[ "$run_code" -ne "$arg" ]]; then
                    echo "program exited with $run_code, expected $arg"; return 1
                fi ;;
            reference)
                translate_and_run "$merged" "$work" || return 1
                echo "exit $run_code" >>"$work/stdout"
                if ! diff -q "$work/stdout" "$CIR2C_REPO_ROOT/$arg" >/dev/null; then
                    echo "output differs from $arg:"
                    diff "$work/stdout" "$CIR2C_REPO_ROOT/$arg" | head -10 | sed 's/^/    /'
                    return 1
                fi ;;
        esac
    done < <(tail -n +2 "$expect")
    return 0
}

for dir in "$SCRIPT_DIR"/cases/*/; do
    dir="${dir%/}"
    name=$(basename "$dir")
    [[ -n "$FILTER" && "$name" != *"$FILTER"* ]] && continue
    work="$WORK/$name"
    mkdir -p "$work"

    sources=()
    if [[ -f "$dir/sources" ]]; then
        while IFS= read -r line; do
            [[ -n "$line" ]] && sources+=("$CIR2C_REPO_ROOT/$line")
        done <"$dir/sources"
    else
        sources=("$dir"/*.c "$dir"/*.cpp "$dir"/*.cir)
    fi

    CASE_CFLAGS="$dir/cflags"
    inputs=()
    emit_failed=""
    for src in "${sources[@]}"; do
        [[ -e "$src" ]] || continue
        if [[ "$src" == *.cir ]]; then
            inputs+=("$src")
            continue
        fi
        out="$work/$(basename "$src").cir"
        emit_cir "$src" "$out" || emit_failed="$src"
        inputs+=("$out")
    done
    if [[ -n "$emit_failed" ]]; then
        echo "FAIL $name: clang could not emit CIR for $(basename "$emit_failed")"
        fail=$((fail + 1))
        continue
    fi

    ok=true
    while read -r -a order; do
        if ! reason=$(check_case "$dir" "$work" "${order[@]}"); then
            names=()
            for f in "${order[@]}"; do names+=("$(basename "$f")"); done
            echo "FAIL $name (inputs in the order ${names[*]}): $reason"
            ok=false
            break
        fi
    done < <(permutations "${inputs[@]}")
    if [[ "$ok" == true ]]; then
        echo "PASS $name"
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
    fi
done

# --- The command line and run-cir2c.sh ---------------------------------------

# check <name> <command...>: the command must succeed.
check() {
    local name="$1"; shift
    if "$@"; then
        echo "PASS $name"; pass=$((pass + 1))
    else
        echo "FAIL $name"; fail=$((fail + 1))
    fi
}

CLI="$WORK/cli"
mkdir -p "$CLI"
CASES="$SCRIPT_DIR/cases"

missing_input() {
    ! "$CIR_MERGE_BIN" -o "$CLI/out.cir" "$CLI/does-not-exist.cir" >"$CLI/log" 2>&1 &&
        grep -q "does-not-exist.cir" "$CLI/log" && [[ ! -e "$CLI/out.cir" ]]
}
check "cli: a missing input is an error" missing_input

failed_merge_keeps_old_output() {
    echo "previous output" >"$CLI/keep.cir"
    ! "$CIR_MERGE_BIN" -o "$CLI/keep.cir" "$CASES/07-target-triple/a.cir" \
        "$CASES/07-target-triple/b.cir" 2>/dev/null &&
        [[ "$(cat "$CLI/keep.cir")" == "previous output" ]] &&
        [[ -z "$(find "$CLI" -name 'keep.cir.tmp-*')" ]]
}
check "cli: a failed merge leaves an existing output untouched" failed_merge_keeps_old_output

dump_symbols() {
    "$CIR_MERGE_BIN" --dump-symbols -o "$CLI/dump.cir" "$CASES/07-target-triple/a.cir" \
        >"$CLI/dump.txt" 2>/dev/null &&
        grep -qP 'a\.cir\tfunction\tdefinition\texternal\t@f$' "$CLI/dump.txt"
}
check "cli: --dump-symbols lists the inventory" dump_symbols

verbose_decisions() {
    "$CIR_MERGE_BIN" --verbose -o "$CLI/verbose.cir" "$CASES/07-target-triple/a.cir" \
        2>"$CLI/verbose.log" && grep -q "@f: unique, kept" "$CLI/verbose.log"
}
check "cli: --verbose explains the decisions" verbose_decisions

pipeline_merges() {
    local d="$CASES/02-declaration-and-definition"
    "$CIR2C_REPO_ROOT/run-cir2c.sh" --merge cir "$d/a.c" "$d/b.c" "$CLI/pipe.c" >/dev/null 2>&1 &&
        "$CIR_CLANG" -w "$CLI/pipe.c" -o "$CLI/pipe" && "$CLI/pipe"
}
check "run-cir2c.sh --merge cir: merges, translates and runs" pipeline_merges

pipeline_rejects() {
    local d="$CASES/04-duplicate-definition"
    "$CIR2C_REPO_ROOT/run-cir2c.sh" --merge cir "$d/a.c" "$d/b.c" "$CLI/rej.c" >"$CLI/rej.log" 2>&1
    local rc=$?
    [[ $rc -eq 5 ]] && grep -q "b.c.cir: duplicate definition of function @f" "$CLI/rej.log"
}
check "run-cir2c.sh --merge cir: exit code 5 names the source file" pipeline_rejects

echo
echo "cir-merge tests: $pass passed, $fail failed"
[[ $fail -eq 0 ]]

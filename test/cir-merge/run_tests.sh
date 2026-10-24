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
#
# Every case is merged with its inputs in sorted and in reverse order: the
# outcome must not depend on the order. A successful merge must also parse and
# verify again with cir-opt.
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
    case "$src" in
        *.cpp) "$CIR_CLANGXX" -std=c++23 -stdlib=libc++ -nostdinc++ -isystem "$CIR_LIBCXX" \
                   -S -emit-cir "$src" -o "$out" 2>/dev/null ;;
        *.c)   "$CIR_CLANG" -std=c23 -S -emit-cir "$src" -o "$out" 2>/dev/null ;;
    esac
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
            run)
                if ! "$CIR2C_BIN" --no-externalize-std "$merged" "$work/out.c" >"$work/cir2c.log" 2>&1; then
                    echo "cir2c failed:"; sed 's/^/    /' "$work/cir2c.log"; return 1
                fi
                if ! "$CIR_CLANG" -w "$work/out.c" -o "$work/out" 2>"$work/cc.log"; then
                    echo "the translated C does not compile:"; sed 's/^/    /' "$work/cc.log"; return 1
                fi
                "$work/out" >/dev/null 2>&1
                local code=$?
                if [[ "$code" -ne "$arg" ]]; then
                    echo "program exited with $code, expected $arg"; return 1
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

    inputs=()
    emit_failed=""
    for src in "$dir"/*.c "$dir"/*.cpp "$dir"/*.cir; do
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

    reversed=()
    for ((i = ${#inputs[@]} - 1; i >= 0; i--)); do reversed+=("${inputs[i]}"); done

    ok=true
    for order in sorted reversed; do
        if [[ "$order" == sorted ]]; then
            reason=$(check_case "$dir" "$work" "${inputs[@]}")
        else
            reason=$(check_case "$dir" "$work" "${reversed[@]}")
        fi
        if [[ $? -ne 0 ]]; then
            echo "FAIL $name ($order inputs): $reason"
            ok=false
            break
        fi
    done
    if [[ "$ok" == true ]]; then
        echo "PASS $name"
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
    fi
done

echo
echo "cir-merge tests: $pass passed, $fail failed"
[[ $fail -eq 0 ]]

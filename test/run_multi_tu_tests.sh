#!/bin/bash
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
#
# Tests with several translation units (README 9.3). Each directory in
# test/multi-tu/ is one test:
#
#   *.c, *.cpp   the translation units
#   args         the mode (tu-part, merge-c or merge-cir) and then the options
#                for run-cir2c.sh, separated by white space
#   expected     "exit N": the linked output runs and exits with N
#                "error TEXT": run-cir2c.sh fails and its output contains TEXT
#
# With tu-part, each unit is translated with --tu-part and the C files are
# linked together. With merge-c and merge-cir, run-cir2c.sh gets all units and
# --merge c or --merge cir.

set -uo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT="$SCRIPT_DIR/.."
CASES_DIR="$SCRIPT_DIR/multi-tu"
OUT_DIR="$SCRIPT_DIR/multi-tu-output"
CC="${CC:-gcc}"
TIMEOUT="${TIMEOUT:-30}"

mkdir -p "$OUT_DIR"
passed=0
failed=0

for case_dir in "$CASES_DIR"/*/; do
    name=$(basename "$case_dir")
    out="$OUT_DIR/$name"
    rm -rf "$out"
    mkdir -p "$out"
    read -r -a args < "$case_dir/args"
    mode="${args[0]}"
    opts=("${args[@]:1}")
    read -r kind value < "$case_dir/expected"
    units=()
    for unit in "$case_dir"*.c "$case_dir"*.cpp; do
        [[ -f "$unit" ]] && units+=("$unit")
    done

    log="$out/pipeline.log"
    rc=0
    c_files=()
    case "$mode" in
        tu-part)
            for unit in "${units[@]}"; do
                c="$out/$(basename "$unit").c"
                "$REPO_ROOT/run-cir2c.sh" "${opts[@]}" --tu-part "$unit" "$c" >>"$log" 2>&1 || rc=$?
                c_files+=("$c")
            done
            ;;
        merge-c|merge-cir)
            c="$out/merged.c"
            "$REPO_ROOT/run-cir2c.sh" "${opts[@]}" --merge "${mode#merge-}" "${units[@]}" "$c" \
                >>"$log" 2>&1 || rc=$?
            c_files+=("$c")
            ;;
        *)
            echo "FAIL  $name (unknown mode: $mode)"
            failed=$((failed + 1))
            continue
            ;;
    esac

    if [[ "$kind" == "error" ]]; then
        if [[ $rc -ne 0 ]] && grep -qF -- "$value" "$log"; then
            echo "PASS  $name"
            passed=$((passed + 1))
        else
            echo "FAIL  $name (expected an error with: $value; exit code $rc)"
            failed=$((failed + 1))
        fi
        continue
    fi

    if [[ $rc -ne 0 ]]; then
        echo "FAIL  $name (run-cir2c.sh exit code $rc, see $log)"
        failed=$((failed + 1))
        continue
    fi
    if ! "$CC" -w -o "$out/a.out" "${c_files[@]}" >>"$log" 2>&1; then
        echo "FAIL  $name (the C files do not compile and link, see $log)"
        failed=$((failed + 1))
        continue
    fi
    timeout -s KILL "$TIMEOUT" "$out/a.out" >"$out/stdout" 2>&1
    actual=$?
    if [[ "$actual" == "$value" ]]; then
        echo "PASS  $name"
        passed=$((passed + 1))
    else
        echo "FAIL  $name (exit code $actual, expected $value)"
        failed=$((failed + 1))
    fi
done

echo "Multi-TU tests: $passed passed, $failed failed"
[[ $failed -eq 0 ]]

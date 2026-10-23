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

# Central CIR pipeline: source file → CIR → preprocess → [flatten] → cir2c → C
#
# C++ input is always compiled against libc++ (-stdlib=libc++), not the
# system's libstdc++: libstdc++ keeps some container internals (e.g.
# std::list's node-splice helpers) as out-of-line symbols ClangIR never sees a
# body for, while libc++ keeps list/map/set/deque fully header-templated. Built
# by docker/build-llvm.sh alongside clang; there is no flag to opt out.
#
# Exit codes: 0 success, 2 clang failed, 3 cir-opt failed, 4 cir2c failed
#
# Usage:
#   Single file:  run-cir2c.sh [OPTIONS] <input-file> <output.c>
#   Multi file:   run-cir2c.sh [OPTIONS] <input1> [input2 ...] --merge c|cir <output.c>
#
# Options:
#   --lang c|c++              Language (default: infer from file extension)
#   --std STD                 Standard (default: c23 or c++23)
#   --flatten                 Run cir-opt -cir-flatten-cfg before cir2c
#   --merge c|cir             Merge strategy for multi-file (default: c)
#   --mlir FILE               Save intermediate CIR MLIR to FILE (default: temp)
#   --flat-mlir FILE          Save flattened MLIR to FILE (implies --flatten)
#   --[no-]externalize-std    Externalize (default) or keep std:: calls

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

# Resolves clang/clang++/cir-opt/libc++ and the cir2c binary; see the script for
# the search order and the CIR2C_LLVM_PREFIX / CIR2C_BIN overrides.
CIR2C_REPO_ROOT="$SCRIPT_DIR"
# shellcheck source=scripts/cir-toolchain.sh
source "$SCRIPT_DIR/scripts/cir-toolchain.sh"

CLANG="$CIR_CLANG"
CLANGPP="$CIR_CLANGXX"
CIR2C="$CIR2C_BIN"

# libc++ (built alongside clang by docker/build-llvm.sh) is the default C++
# standard library for CIR generation: unlike libstdc++, it keeps list/map/
# set/deque entirely header-templated, so ClangIR sees real bodies instead of
# bodiless externs for container internals (e.g. std::list's node-splice
# helpers) — see the no-externalize-std limitation notes in cir2c.
LIBCXX_INCLUDE="$CIR_LIBCXX"

LANG=""
STD=""
FLATTEN=false
MERGE_STRATEGY="c"
MLIR_OUT=""
FLAT_MLIR_OUT=""
INCLUDE_FLAGS=()   # -I dirs for CIR generation, supplied by the caller
EXTERNALIZE_STD=true

usage() {
    echo "Usage: $0 [OPTIONS] <input-file(s)> <output.c>" >&2
    echo "  --lang c|c++, --std STD, --flatten" >&2
    echo "  --merge c|cir   merge strategy for multi-file (default: c)" >&2
    echo "  --mlir FILE, --flat-mlir FILE" >&2
    echo "  --include DIR   add -I DIR to CIR generation (repeatable)" >&2
    echo "  --[no-]externalize-std" >&2
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --lang)               LANG="$2"; shift 2 ;;
        --std)                STD="$2"; shift 2 ;;
        --flatten)            FLATTEN=true; shift ;;
        --merge)              MERGE_STRATEGY="$2"; shift 2 ;;
        --mlir)               MLIR_OUT="$2"; shift 2 ;;
        --flat-mlir)          FLAT_MLIR_OUT="$2"; FLATTEN=true; shift 2 ;;
        --include)            INCLUDE_FLAGS+=(-I "$2"); shift 2 ;;
        --externalize-std)    EXTERNALIZE_STD=true; shift ;;
        --no-externalize-std) EXTERNALIZE_STD=false; shift ;;
        --) shift; break ;;
        -*) usage ;;
        *) break ;;
    esac
done

# Parse positional arguments: last is output, all others are inputs
[[ $# -lt 2 ]] && usage
OUTPUT_C="${!#}"
INPUT_FILES=("$@")
unset 'INPUT_FILES[$#]'

# Validate all input files exist
for f in "${INPUT_FILES[@]}"; do
    [[ -f "$f" ]] || { echo "Error: input file not found: $f" >&2; exit 1; }
done

NUM_INPUTS=${#INPUT_FILES[@]}

# Infer language from extension (use first input file)
if [[ -z "$LANG" ]]; then
    case "${INPUT_FILES[0]##*.}" in
        cpp|cxx|cc|C) LANG="c++" ;;
        *) LANG="c" ;;
    esac
fi
[[ -z "$STD" ]] && { [[ "$LANG" == "c++" ]] && STD="c++23" || STD="c23"; }

CLANG_BIN="$CLANG"
STDLIB_FLAGS=()
if [[ "$LANG" == "c++" ]]; then
    CLANG_BIN="$CLANGPP"
    STDLIB_FLAGS=(-stdlib=libc++ -nostdinc++ -isystem "$LIBCXX_INCLUDE")
fi

# Validate tools
REQUIRE_FLAGS=()
[[ "$FLATTEN" == true ]] && REQUIRE_FLAGS+=(--cir-opt)
[[ "$LANG" == "c++" ]] && REQUIRE_FLAGS+=(--libcxx)
cir_require_toolchain ${REQUIRE_FLAGS[@]+"${REQUIRE_FLAGS[@]}"} || exit 1

# Temp file management
_TEMPS=()
_cleanup() { rm -f "${_TEMPS[@]:-}"; }
trap _cleanup EXIT

_tmpmlir() {
    if [[ -n "$1" ]]; then echo "$1"; return; fi
    local t; t=$(mktemp --suffix=.mlir); _TEMPS+=("$t"); echo "$t"
}

_tmpc() {
    local t; t=$(mktemp --suffix=.c); _TEMPS+=("$t"); echo "$t"
}

mkdir -p "$(dirname "$OUTPUT_C")"

# Helper: run the full cir2c pipeline on a single file
# Args: input_file output_c
run_single_pipeline() {
    local input="$1"
    local output="$2"

    # Step 1: Generate CIR
    local cir_file
    cir_file=$(_tmpmlir "")
    "$CLANG_BIN" -x "$LANG" "-std=$STD" -S -emit-cir "$input" -o "$cir_file" \
        ${STDLIB_FLAGS[@]+"${STDLIB_FLAGS[@]}"} \
        ${INCLUDE_FLAGS[@]+"${INCLUDE_FLAGS[@]}"} || exit 2

    # Step 2: Preprocess
    local pre_file
    pre_file=$(_tmpmlir "")
    sed -e 's/, cleanup_dest_slot\]/\]/g' \
        -e 's/, const\]/\]/g' \
        "$cir_file" > "$pre_file"

    # Step 3: Flatten (optional)
    local mapper_input
    if [[ "$FLATTEN" == true ]]; then
        local flat_file
        flat_file=$(_tmpmlir "")
        "$CIR_OPT" "$pre_file" -cir-flatten-cfg -o "$flat_file" || exit 3
        mapper_input="$flat_file"
    else
        mapper_input="$pre_file"
    fi

    # Step 4: cir2c
    local mapper_cmd=("$CIR2C")
    [[ "$EXTERNALIZE_STD" == true ]] && mapper_cmd+=(--externalize-std) || mapper_cmd+=(--no-externalize-std)
    mapper_cmd+=("$mapper_input" "$output")
    "${mapper_cmd[@]}" || exit 4
}

# Helper: compile a single file to CIR MLIR
# Args: input_file output_mlir
compile_to_cir() {
    local input="$1"
    local output="$2"

    "$CLANG_BIN" -x "$LANG" "-std=$STD" -S -emit-cir "$input" -o "$output" \
        ${STDLIB_FLAGS[@]+"${STDLIB_FLAGS[@]}"} \
        ${INCLUDE_FLAGS[@]+"${INCLUDE_FLAGS[@]}"} || exit 2
}

if [[ "$NUM_INPUTS" -eq 1 ]]; then
    # Single-file path: backward compatible
    INPUT_FILE="${INPUT_FILES[0]}"

    # Step 1: Generate CIR
    CIR_FILE=$(_tmpmlir "$MLIR_OUT")
    "$CLANG_BIN" -x "$LANG" "-std=$STD" -S -emit-cir "$INPUT_FILE" -o "$CIR_FILE" \
        ${STDLIB_FLAGS[@]+"${STDLIB_FLAGS[@]}"} \
        ${INCLUDE_FLAGS[@]+"${INCLUDE_FLAGS[@]}"} || exit 2

    # Step 2: Preprocess
    PRE_FILE=$(_tmpmlir "")
    sed -e 's/, cleanup_dest_slot\]/\]/g' \
        -e 's/, const\]/\]/g' \
        "$CIR_FILE" > "$PRE_FILE"

    # Step 3: Flatten (optional)
    if [[ "$FLATTEN" == true ]]; then
        FLAT_FILE=$(_tmpmlir "$FLAT_MLIR_OUT")
        "$CIR_OPT" "$PRE_FILE" -cir-flatten-cfg -o "$FLAT_FILE" || exit 3
        MAPPER_INPUT="$FLAT_FILE"
    else
        MAPPER_INPUT="$PRE_FILE"
    fi

    # Step 4: cir2c
    MAPPER_CMD=("$CIR2C")
    [[ "$EXTERNALIZE_STD" == true ]] && MAPPER_CMD+=(--externalize-std) || MAPPER_CMD+=(--no-externalize-std)
    MAPPER_CMD+=("$MAPPER_INPUT" "$OUTPUT_C")
    "${MAPPER_CMD[@]}" || exit 4

else
    # Multi-file path
    case "$MERGE_STRATEGY" in
        c)
            # C-level merge: run pipeline on each file, then merge the C outputs
            C_FILES=()
            for input_file in "${INPUT_FILES[@]}"; do
                tmp_c=$(_tmpc)
                run_single_pipeline "$input_file" "$tmp_c"
                C_FILES+=("$tmp_c")
            done
            # Merge all C files into the final output
            "$SCRIPT_DIR/scripts/merge-c.sh" "${C_FILES[@]}" "$OUTPUT_C"
            ;;
        cir)
            # CIR-level merge: compile each to CIR, merge MLIR, then run cir2c once
            MLIR_FILES=()
            for input_file in "${INPUT_FILES[@]}"; do
                tmp_mlir=$(_tmpmlir "")
                compile_to_cir "$input_file" "$tmp_mlir"
                MLIR_FILES+=("$tmp_mlir")
            done
            # Merge all MLIR files
            MERGED_MLIR=$(_tmpmlir "")
            "$SCRIPT_DIR/scripts/merge-cir.sh" "${MLIR_FILES[@]}" "$MERGED_MLIR"
            # Preprocess the merged MLIR
            PRE_FILE=$(_tmpmlir "")
            sed -e 's/, cleanup_dest_slot\]/\]/g' \
                -e 's/, const\]/\]/g' \
                "$MERGED_MLIR" > "$PRE_FILE"
            # Flatten (optional)
            if [[ "$FLATTEN" == true ]]; then
                FLAT_FILE=$(_tmpmlir "$FLAT_MLIR_OUT")
                "$CIR_OPT" "$PRE_FILE" -cir-flatten-cfg -o "$FLAT_FILE" || exit 3
                MAPPER_INPUT="$FLAT_FILE"
            else
                MAPPER_INPUT="$PRE_FILE"
            fi
            # cir2c
            MAPPER_CMD=("$CIR2C")
            [[ "$EXTERNALIZE_STD" == true ]] && MAPPER_CMD+=(--externalize-std) || MAPPER_CMD+=(--no-externalize-std)
            MAPPER_CMD+=("$MAPPER_INPUT" "$OUTPUT_C")
            "${MAPPER_CMD[@]}" || exit 4
            ;;
        *)
            echo "Error: unknown merge strategy: $MERGE_STRATEGY (use 'c' or 'cir')" >&2
            exit 1
            ;;
    esac
fi

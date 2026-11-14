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

# Central CIR pipeline: source file → CIR → preprocess → cir2c → C
#
# C++ input is always compiled against libc++ (-stdlib=libc++), not the
# system's libstdc++: libstdc++ keeps some container internals (e.g.
# std::list's node-splice helpers) as out-of-line symbols ClangIR never sees a
# body for, while libc++ keeps list/map/set/deque fully header-templated. Built
# by docker/build-llvm.sh alongside clang; there is no flag to opt out.
#
# With --no-externalize-std, C++ input is also compiled with the models in
# models/ (README 8.2): C++ headers that define the parts of libc++ that libc++
# keeps in its compiled library, so that the CIR contains their bodies.
#
# Exit codes: 0 success, 1 wrong arguments or no toolchain, 2 clang failed,
# 3 cir-opt failed (--merge cir), 4 cir2c failed, 5 cir-merge rejected the inputs (--merge cir)
#
# Usage:
#   Single file:  run-cir2c.sh [OPTIONS] <input-file> <output.c>
#   Multi file:   run-cir2c.sh [OPTIONS] <input1> [input2 ...] --merge c|cir <output.c>
#
# Options:
#   --lang c|c++              Language (default: infer from file extension)
#   --std STD                 Standard (default: c23 or c++23)
#   --merge c|cir             Merge strategy for multi-file (default: c)
#   --mlir FILE               Save intermediate CIR MLIR to FILE (default: temp)
#   --cflag FLAG              Add FLAG to the CIR generation step (repeatable)
#   --[no-]externalize-std    Externalize (default) or keep std:: calls
#   --tu-part                 The output is one translation unit of several; it
#                             keeps the C linkage of its symbols, to be linked
#   --no-call-conv-lowering   Skip ClangIR's calling-convention lowering, for types
#                             it cannot lower yet. UNSOUND: an object with a move
#                             constructor is then passed by value as a bitwise copy,
#                             so the caller never sees it moved from and destroys it.
#                             Use it for all translation units of a program or none.

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
MERGE_STRATEGY="c"
MLIR_OUT=""
INCLUDE_FLAGS=()   # -I dirs for CIR generation, supplied by the caller
EXTRA_CFLAGS=()    # further flags for CIR generation (--cflag), e.g. -D
EXTERNALIZE_STD=true
TU_PART=()
CIR_CFLAGS=()

usage() {
    echo "Usage: $0 [OPTIONS] <input-file(s)> <output.c>" >&2
    echo "  --lang c|c++, --std STD" >&2
    echo "  --merge c|cir   merge strategy for multi-file (default: c)" >&2
    echo "  --mlir FILE" >&2
    echo "  --include DIR   add -I DIR to CIR generation (repeatable)" >&2
    echo "  --cflag FLAG    add FLAG to CIR generation (repeatable)" >&2
    echo "  --[no-]externalize-std, --tu-part, --no-call-conv-lowering" >&2
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --lang)               LANG="$2"; shift 2 ;;
        --std)                STD="$2"; shift 2 ;;
        --merge)              MERGE_STRATEGY="$2"; shift 2 ;;
        --mlir)               MLIR_OUT="$2"; shift 2 ;;
        --include)            INCLUDE_FLAGS+=(-I "$2"); shift 2 ;;
        --cflag)              EXTRA_CFLAGS+=("$2"); shift 2 ;;
        --externalize-std)    EXTERNALIZE_STD=true; shift ;;
        --no-externalize-std) EXTERNALIZE_STD=false; shift ;;
        --tu-part)            TU_PART=(--tu-part); shift ;;
        --no-call-conv-lowering) CIR_CFLAGS=(-Xclang -fno-clangir-call-conv-lowering); shift ;;
        --) shift; break ;;
        -*) usage ;;
        *) break ;;
    esac
done

# Parse positional arguments: last is output, all others are inputs
[[ $# -lt 2 ]] && usage
OUTPUT_C="${!#}"
INPUT_FILES=("$@")
unset 'INPUT_FILES[$(( $# - 1 ))]'

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
    # The output C has one thread. Without thread-safe statics, clang guards a
    # static local variable with a plain flag. Otherwise it calls the
    # __cxa_guard_* functions of the C++ runtime, which the output does not have.
    STDLIB_FLAGS=(-stdlib=libc++ -nostdinc++ -isystem "$LIBCXX_INCLUDE"
                  -fno-threadsafe-statics)
fi

# The models for --no-externalize-std (README 8.2). A model is a directory in
# models/ with an include/ directory, a prelude.h file, or both. clang searches
# the -I directory of include/ before the -isystem directory of libc++, so a
# header there replaces the libc++ header of the same name. clang reads
# prelude.h before the first line of the program. To use other models, change
# this list.
MODELS=(libcxx-out-of-line ostream-sink istream-nondet)
MODEL_FLAGS=()
if [[ "$LANG" == "c++" && "$EXTERNALIZE_STD" == false ]]; then
    declare -A MODEL_HEADERS=()
    for model in "${MODELS[@]}"; do
        dir="$SCRIPT_DIR/models/$model"
        [[ -d "$dir" ]] || { echo "Error: model not found: $dir" >&2; exit 1; }
        if [[ -d "$dir/include" ]]; then
            # Two models with one header would depend on the order of the list.
            for header in "$dir"/include/*; do
                name=$(basename "$header")
                if [[ -n "${MODEL_HEADERS[$name]:-}" ]]; then
                    echo "Error: models ${MODEL_HEADERS[$name]} and $model both have <$name>" >&2
                    exit 1
                fi
                MODEL_HEADERS[$name]=$model
            done
            MODEL_FLAGS+=(-I "$dir/include")
        fi
        [[ -f "$dir/prelude.h" ]] && MODEL_FLAGS+=(-include "$dir/prelude.h")
    done
fi

# __FILE__ expands to the path of the file as clang finds it, and the output
# keeps it, for example in the message of an assertion. Without the directory,
# the output does not contain a local path, and it does not change with the
# directory of the translation.
# Each input file gets the directories of its own path.
set_cir_gen_flags() {
    local input="$1"
    CIR_GEN_FLAGS=()
    local dir
    for dir in "$(dirname -- "$input")" "$(cd -- "$(dirname -- "$input")" && pwd -P)" \
               "$SCRIPT_DIR/models" ${LIBCXX_INCLUDE:+"$LIBCXX_INCLUDE"}; do
        CIR_GEN_FLAGS+=("-fmacro-prefix-map=${dir%/}/=")
    done
    CIR_GEN_FLAGS+=(${MODEL_FLAGS[@]+"${MODEL_FLAGS[@]}"} ${STDLIB_FLAGS[@]+"${STDLIB_FLAGS[@]}"})
}

# --merge c translates each unit alone, so the unit with main() would fail the
# definition check of --no-externalize-std for each symbol that another unit
# defines (README 8.2). --merge cir checks the merged module.
if [[ "$NUM_INPUTS" -gt 1 && "$MERGE_STRATEGY" == "c" && "$LANG" == "c++" && "$EXTERNALIZE_STD" == false ]]; then
    echo "Error: --merge c cannot check the definitions of --no-externalize-std for C++ input. Use --merge cir." >&2
    exit 1
fi

# Validate tools
REQUIRE_FLAGS=()
[[ "$LANG" == "c++" ]] && REQUIRE_FLAGS+=(--libcxx)
[[ "$NUM_INPUTS" -gt 1 && "$MERGE_STRATEGY" == "cir" ]] && REQUIRE_FLAGS+=(--cir-merge --cir-opt)
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
    set_cir_gen_flags "$input"
    "$CLANG_BIN" -x "$LANG" "-std=$STD" -S -emit-cir "$input" -o "$cir_file" \
        "${CIR_GEN_FLAGS[@]}" \
        ${INCLUDE_FLAGS[@]+"${INCLUDE_FLAGS[@]}"} \
        ${CIR_CFLAGS[@]+"${CIR_CFLAGS[@]}"} ${EXTRA_CFLAGS[@]+"${EXTRA_CFLAGS[@]}"} || exit 2

    # Step 2: Preprocess
    local pre_file
    pre_file=$(_tmpmlir "")
    sed -e '/cir\.alloca/s/, cleanup_dest_slot\]/\]/g' \
        -e '/cir\.alloca/s/, const\]/\]/g' \
        "$cir_file" > "$pre_file"

    # Step 3: cir2c
    local mapper_input="$pre_file"
    local mapper_cmd=("$CIR2C")
    [[ "$EXTERNALIZE_STD" == true ]] && mapper_cmd+=(--externalize-std) || mapper_cmd+=(--no-externalize-std)
    mapper_cmd+=(${TU_PART[@]+"${TU_PART[@]}"})
    mapper_cmd+=("$mapper_input" "$output")
    "${mapper_cmd[@]}" || exit 4
}

# Helper: compile a single file to CIR MLIR
# Args: input_file output_mlir
compile_to_cir() {
    local input="$1"
    local output="$2"

    # Unlowered, whatever the options say: lowered modules differ from each other
    # (the temporaries of a by-value parameter come in any order, and a parameter
    # class that is incomplete in one unit lowers another way), so ODR-equivalent
    # definitions would not compare equal. The merged module is lowered once.
    set_cir_gen_flags "$input"
    "$CLANG_BIN" -x "$LANG" "-std=$STD" -S -emit-cir "$input" -o "$output" \
        "${CIR_GEN_FLAGS[@]}" \
        ${INCLUDE_FLAGS[@]+"${INCLUDE_FLAGS[@]}"} \
        -Xclang -fno-clangir-call-conv-lowering ${EXTRA_CFLAGS[@]+"${EXTRA_CFLAGS[@]}"} || exit 2
}

if [[ "$NUM_INPUTS" -eq 1 ]]; then
    # Single-file path: backward compatible
    INPUT_FILE="${INPUT_FILES[0]}"

    # Step 1: Generate CIR
    CIR_FILE=$(_tmpmlir "$MLIR_OUT")
    set_cir_gen_flags "$INPUT_FILE"
    "$CLANG_BIN" -x "$LANG" "-std=$STD" -S -emit-cir "$INPUT_FILE" -o "$CIR_FILE" \
        "${CIR_GEN_FLAGS[@]}" \
        ${INCLUDE_FLAGS[@]+"${INCLUDE_FLAGS[@]}"} \
        ${CIR_CFLAGS[@]+"${CIR_CFLAGS[@]}"} ${EXTRA_CFLAGS[@]+"${EXTRA_CFLAGS[@]}"} || exit 2

    # Step 2: Preprocess
    PRE_FILE=$(_tmpmlir "")
    sed -e '/cir\.alloca/s/, cleanup_dest_slot\]/\]/g' \
        -e '/cir\.alloca/s/, const\]/\]/g' \
        "$CIR_FILE" > "$PRE_FILE"

    # Step 3: cir2c
    MAPPER_INPUT="$PRE_FILE"
    MAPPER_CMD=("$CIR2C")
    [[ "$EXTERNALIZE_STD" == true ]] && MAPPER_CMD+=(--externalize-std) || MAPPER_CMD+=(--no-externalize-std)
    MAPPER_CMD+=(${TU_PART[@]+"${TU_PART[@]}"})
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
                # Name the temporary after its source, so that cir-merge's
                # diagnostics say which translation unit they are about.
                tmp_mlir=$(mktemp --suffix="-$(basename "$input_file").cir")
                _TEMPS+=("$tmp_mlir")
                compile_to_cir "$input_file" "$tmp_mlir"
                MLIR_FILES+=("$tmp_mlir")
            done
            # Merge all MLIR files. cir-merge is conservative: it rejects
            # what it cannot merge soundly (see "Merging translation units"
            # in README.md) instead of producing an approximate module.
            MERGED_MLIR=$(_tmpmlir "")
            "$CIR_MERGE_BIN" --output "$MERGED_MLIR" "${MLIR_FILES[@]}" || exit 5
            if [[ ${#CIR_CFLAGS[@]} -eq 0 ]]; then
                LOWERED_MLIR=$(_tmpmlir "")
                "$CIR_OPT" "$MERGED_MLIR" --cir-call-conv-lowering=target=x86_64 -o "$LOWERED_MLIR" || exit 3
                MERGED_MLIR="$LOWERED_MLIR"
            fi
            # Preprocess the merged MLIR
            PRE_FILE=$(_tmpmlir "")
            sed -e '/cir\.alloca/s/, cleanup_dest_slot\]/\]/g' \
                -e '/cir\.alloca/s/, const\]/\]/g' \
                "$MERGED_MLIR" > "$PRE_FILE"
            MAPPER_INPUT="$PRE_FILE"
            # cir2c
            MAPPER_CMD=("$CIR2C")
            [[ "$EXTERNALIZE_STD" == true ]] && MAPPER_CMD+=(--externalize-std) || MAPPER_CMD+=(--no-externalize-std)
    MAPPER_CMD+=(${TU_PART[@]+"${TU_PART[@]}"})
            MAPPER_CMD+=("$MAPPER_INPUT" "$OUTPUT_C")
            "${MAPPER_CMD[@]}" || exit 4
            ;;
        *)
            echo "Error: unknown merge strategy: $MERGE_STRATEGY (use 'c' or 'cir')" >&2
            exit 1
            ;;
    esac
fi

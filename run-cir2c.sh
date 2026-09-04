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
# Usage: run-cir2c.sh [OPTIONS] <input-file> <output.c>
# Options:
#   --lang c|c++              Language (default: infer from file extension)
#   --std STD                 Standard (default: c23 or c++23)
#   --flatten                 Run cir-opt -cir-flatten-cfg before cir2c
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
MLIR_OUT=""
FLAT_MLIR_OUT=""
INCLUDE_FLAGS=()   # -I dirs for CIR generation, supplied by the caller
EXTERNALIZE_STD=true

usage() {
    echo "Usage: $0 [OPTIONS] <input-file> <output.c>" >&2
    echo "  --lang c|c++, --std STD, --flatten" >&2
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

[[ $# -ne 2 ]] && usage
INPUT_FILE="$1"
OUTPUT_C="$2"
[[ -f "$INPUT_FILE" ]] || { echo "Error: input file not found: $INPUT_FILE" >&2; exit 1; }

# Infer language from extension
if [[ -z "$LANG" ]]; then
    case "${INPUT_FILE##*.}" in
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

mkdir -p "$(dirname "$OUTPUT_C")"

# Step 1: Generate CIR.
# Include dirs (-I) come from the caller via --include (see run_tests.sh, which
# knows the test-suite layout for shared utility headers).
CIR_FILE=$(_tmpmlir "$MLIR_OUT")
"$CLANG_BIN" -x "$LANG" "-std=$STD" -S -emit-cir "$INPUT_FILE" -o "$CIR_FILE" \
    ${STDLIB_FLAGS[@]+"${STDLIB_FLAGS[@]}"} \
    ${INCLUDE_FLAGS[@]+"${INCLUDE_FLAGS[@]}"} || exit 2

# Step 2: Preprocess — strip alloca qualifiers that ClangIR's AllocaOp parser
# used to reject: ", cleanup_dest_slot" (C++ EH slot) and ", const"
# (const-qualified catch variable). LLVM >= 23 spells these without commas and
# parses them fine, so this is a no-op there; it is kept for CIR text produced
# by an older toolchain. Safe either way — the C emitter ignores both flags.
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

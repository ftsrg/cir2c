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

# Shared toolchain discovery, sourced by run-cir2c.sh and test/run_tests.sh.
#
# Locates the ClangIR-enabled LLVM install and the cir2c binary, and exports:
#
#   CIR_PREFIX     install prefix of the LLVM toolchain
#   CIR_CLANG      $CIR_PREFIX/bin/clang
#   CIR_CLANGXX    $CIR_PREFIX/bin/clang++
#   CIR_OPT        $CIR_PREFIX/bin/cir-opt
#   CIR_LIBCXX     $CIR_PREFIX/include/c++/v1   (may not exist)
#   CIR2C_BIN      the cir2c executable
#
# Overrides, in precedence order:
#   $CIR2C_LLVM_PREFIX   toolchain prefix
#   $CIR2C_BIN           cir2c executable
#   $CIR2C_BUILD_DIR     build directory to take cir2c from (default <repo>/build)
#
# Everything is resolved relative to $CIR2C_REPO_ROOT, which the sourcing script
# must set before sourcing this file.

: "${CIR2C_REPO_ROOT:?cir-toolchain.sh: CIR2C_REPO_ROOT must be set before sourcing}"

# --- LLVM toolchain --------------------------------------------------------
_cir_prefix_candidates=()
[[ -n "${CIR2C_LLVM_PREFIX:-}" ]] && _cir_prefix_candidates+=("$CIR2C_LLVM_PREFIX")
_cir_prefix_candidates+=("$CIR2C_REPO_ROOT/llvm-install" "/opt/cir")

CIR_PREFIX=""
for _c in "${_cir_prefix_candidates[@]}"; do
    if [[ -x "$_c/bin/clang" ]]; then
        CIR_PREFIX="$_c"
        break
    fi
done

# Last resort: a clang on PATH. Whether it is ClangIR-enabled is checked lazily
# by cir_require_toolchain below.
if [[ -z "$CIR_PREFIX" ]] && command -v clang >/dev/null 2>&1; then
    CIR_PREFIX="$(cd "$(dirname "$(command -v clang)")/.." && pwd)"
fi

CIR_CLANG="$CIR_PREFIX/bin/clang"
CIR_CLANGXX="$CIR_PREFIX/bin/clang++"
CIR_OPT="$CIR_PREFIX/bin/cir-opt"
CIR_LIBCXX="$CIR_PREFIX/include/c++/v1"

# --- cir2c binary ----------------------------------------------------------
if [[ -z "${CIR2C_BIN:-}" ]]; then
    CIR2C_BIN="${CIR2C_BUILD_DIR:-$CIR2C_REPO_ROOT/build}/cir2c"
fi

export CIR_PREFIX CIR_CLANG CIR_CLANGXX CIR_OPT CIR_LIBCXX CIR2C_BIN

# cir_require_toolchain [--cir-opt] [--libcxx]
#
# Fails with an actionable message unless clang and cir2c are present (and,
# when asked for, cir-opt and the libc++ headers).
cir_require_toolchain() {
    local need_libcxx=false
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --cir-opt) ;;   # cir-opt is required unconditionally, see below
            --libcxx)  need_libcxx=true ;;
        esac
        shift
    done

    if [[ ! -x "$CIR_CLANG" ]]; then
        cat >&2 <<EOF
Error: no ClangIR-enabled clang found.

Looked in: ${_cir_prefix_candidates[*]}

Build one with:      ./docker/build-llvm.sh --prefix ./llvm-install
or pull the image:   docker pull ghcr.io/ftsrg/cir2c-llvm:latest
or point at yours:   export CIR2C_LLVM_PREFIX=/path/to/llvm-install

See docs/building.md.
EOF
        return 1
    fi

    # cir-opt is only built when CLANG_ENABLE_CIR=ON, so its presence next to
    # clang is a cheap, reliable proxy for "this toolchain has ClangIR". Probing
    # clang itself would mean spawning it, and run_tests.sh sources this file
    # once per test case.
    if [[ ! -x "$CIR_OPT" ]]; then
        echo "Error: $CIR_PREFIX has clang but no cir-opt, so it was almost certainly" >&2
        echo "built without -DCLANG_ENABLE_CIR=ON and cannot emit CIR." >&2
        echo "See docs/building.md." >&2
        return 1
    fi

    if [[ ! -x "$CIR2C_BIN" ]]; then
        echo "Error: cir2c binary not found: $CIR2C_BIN" >&2
        echo "Build it with: cmake -B build && cmake --build build" >&2
        echo "Or set CIR2C_BIN / CIR2C_BUILD_DIR." >&2
        return 1
    fi

    if [[ "$need_libcxx" == true && ! -d "$CIR_LIBCXX" ]]; then
        echo "Error: libc++ headers not found: $CIR_LIBCXX" >&2
        echo "They are installed alongside clang by docker/build-llvm.sh." >&2
        return 1
    fi

    return 0
}

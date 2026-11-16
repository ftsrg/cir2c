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

# Per-program settings of the LLVM MultiSource tests, sourced by
# test/run_tests.sh. They come from each program's CMakeLists.txt in
# llvm-test-suite (https://github.com/llvm/llvm-test-suite), which the
# reference outputs were made with; the default problem size is used.
#
# multisource_options <program directory, relative to MultiSource/> sets:
#   MS_STD           language standard (--std), empty for the default
#   MS_CFLAGS        further flags for the CIR step (array, --cflag)
#   MS_RUN_ARGS      program arguments (array; paths relative to the
#                    program directory, where the program runs)
#   MS_STDIN         file in the program directory to feed on stdin
#   MS_FP_TOLERANCE  relative tolerance for numbers in the output (fpcmp -r)

multisource_options() {
    MS_STD=""
    MS_CFLAGS=()
    MS_RUN_ARGS=()
    MS_STDIN=""
    MS_FP_TOLERANCE=""
    case "$1" in
        Applications/aha)
            MS_STD=c17 ;;
        Applications/hexxagon)
            MS_STD=c++14
            MS_STDIN=input ;;
        Applications/sgefa)
            MS_STD=c17
            MS_CFLAGS=(-Wno-implicit-int -DSCALE=40)
            MS_FP_TOLERANCE=0.0001 ;;
        Benchmarks/FreeBench/*)
            MS_CFLAGS=('-DVERSION="1.00"' '-DCOMPDATE="today"' '-DCFLAGS=""'
                       '-DHOSTNAME="thishost"')
            MS_RUN_ARGS=(test.in) ;;
        Benchmarks/Trimaran/enc-3des)
            MS_RUN_ARGS=(100) ;;
        Benchmarks/Trimaran/enc-md5)
            MS_RUN_ARGS=(10) ;;
    esac
}

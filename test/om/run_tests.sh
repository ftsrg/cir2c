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

# Runs each program here natively and translated against the operational models,
# and compares their output. Symbols from the compiled libc++ have no body in the
# translated program, so they may stay unresolved as long as they are not called.

set -uo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
echo 'int __VERIFIER_nondet_int(void) { return 0; }' > "$TMP/rt.c"
failed=0
for src in "$ROOT"/test/om/*.cpp; do
  name=$(basename "$src" .cpp)
  c++ -std=c++17 -w "$src" -pthread -o "$TMP/$name.native" && "$TMP/$name.native" > "$TMP/$name.expected"
  if "$ROOT/run-cir2c.sh" --std c++17 --no-externalize-std --include "$ROOT/libcxx-om/c++/v1" "$src" "$TMP/$name.c" > "$TMP/$name.log" 2>&1 &&
     cc -w -std=gnu11 -no-pie "$TMP/$name.c" "$TMP/rt.c" -lm -pthread -Wl,--unresolved-symbols=ignore-all -o "$TMP/$name" &&
     "$TMP/$name" > "$TMP/$name.actual" &&
     diff "$TMP/$name.expected" "$TMP/$name.actual"; then
    echo "PASS $name"
  else
    echo "FAIL $name"
    tail -5 "$TMP/$name.log"
    failed=1
  fi
done
exit $failed

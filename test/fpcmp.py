#!/usr/bin/env python3
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

"""Compare two program outputs like LLVM's fpcmp -r TOLERANCE.

The text must be equal, except that two numbers at the same position may
differ by the relative tolerance. Exit code 0 if the outputs match, 1 if not.

Usage: fpcmp.py TOLERANCE EXPECTED ACTUAL
"""

import re
import sys

NUMBER = re.compile(r"[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?")


def tokens(text):
    """Split text into alternating non-number and number tokens."""
    result = []
    pos = 0
    for match in NUMBER.finditer(text):
        result.append(("text", text[pos:match.start()]))
        result.append(("number", match.group()))
        pos = match.end()
    result.append(("text", text[pos:]))
    return result


def numbers_match(a, b, tolerance):
    x, y = float(a), float(b)
    if x == y:
        return True
    return abs(x - y) <= tolerance * max(abs(x), abs(y))


def main():
    tolerance = float(sys.argv[1])
    with open(sys.argv[2], errors="replace") as f:
        expected = tokens(f.read())
    with open(sys.argv[3], errors="replace") as f:
        actual = tokens(f.read())
    if len(expected) != len(actual):
        return 1
    for (kind_e, e), (kind_a, a) in zip(expected, actual):
        if kind_e != kind_a:
            return 1
        if kind_e == "text" and e != a:
            return 1
        if kind_e == "number" and not numbers_match(e, a, tolerance):
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

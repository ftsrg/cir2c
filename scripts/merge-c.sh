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

# Merge multiple C files into a single C file by extracting top-level
# definitions (functions, globals, structs, typedefs, enums) and
# deduplicating by name (first wins).
#
# Usage: merge-c.sh <input1.c> [input2.c ...] <output.c>
#
# The last argument is the output file; all preceding arguments are inputs.

set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "Usage: $0 <input1.c> [input2.c ...] <output.c>" >&2
    exit 1
fi

# Last argument is the output; everything before it is input
OUTPUT="${!#}"
INPUTS=("$@")
unset 'INPUTS[$#]'

# If only one input, just copy it
if [[ ${#INPUTS[@]} -eq 1 ]]; then
    cp "${INPUTS[0]}" "$OUTPUT"
    exit 0
fi

# We use a Python helper for reliable C parsing since bash can't handle
# nested braces, preprocessor directives, and multi-line declarations
# robustly.

python3 - "$OUTPUT" "${INPUTS[@]}" << 'PYTHON_SCRIPT'
import sys
import re
import os

def extract_top_level_blocks(lines):
    """
    Extract top-level blocks from C code. A top-level block is a function
    definition, struct/enum/union definition, typedef, or global variable
    declaration. We track brace depth to find block boundaries.
    
    Returns a list of (name, block_text) tuples.
    """
    blocks = []
    i = 0
    n = len(lines)
    
    while i < n:
        line = lines[i]
        stripped = line.strip()
        
        # Skip empty lines and comments at top level
        if not stripped or stripped.startswith('//') or stripped.startswith('/*') or stripped.startswith('*'):
            i += 1
            continue
        
        # Skip preprocessor directives (collect them separately)
        if stripped.startswith('#'):
            i += 1
            continue
        
        # Try to identify the start of a top-level definition
        # We look for patterns that start a block:
        # - Function: type name(...) {
        # - Struct/enum/union: struct/enum/union name {
        # - Typedef: typedef ... ;
        # - Global variable: type name = ...; or type name;
        
        # Check for struct/enum/union/typedef
        block_start = None
        name = None
        
        # struct/enum/union definition
        m = re.match(r'\s*(?:typedef\s+)?(struct|enum|union)\s+(\w+)', stripped)
        if m:
            block_start = i
            name = m.group(2)
        
        # Function definition: type name(...)
        elif re.match(r'\s*(?:static|inline|extern|unsigned|signed|const|volatile|long|short|void|int|char|float|double|size_t|uintptr_t|intptr_t|bool|_Bool|__attribute__|\w+)\s+\w+\s*\(', stripped):
            # Extract function name
            m2 = re.search(r'(\w+)\s*\(', stripped)
            if m2:
                fname = m2.group(1)
                # Skip control flow keywords
                if fname not in ('if', 'while', 'for', 'switch', 'return', 'else', 'do'):
                    block_start = i
                    name = fname
        
        if block_start is not None:
            # Find the end of this block
            brace_depth = 0
            block_lines = []
            found_open = False
            j = block_start
            while j < n:
                block_lines.append(lines[j])
                for ch in lines[j]:
                    if ch == '{':
                        brace_depth += 1
                        found_open = True
                    elif ch == '}':
                        brace_depth -= 1
                j += 1
                if found_open and brace_depth == 0:
                    break
            
            # If no braces found, it might be a declaration ending with ;
            if not found_open:
                # Look for semicolon at top level
                j = block_start
                while j < n:
                    block_lines.append(lines[j])
                    if ';' in lines[j]:
                        j += 1
                        break
                    j += 1
            
            block_text = '\n'.join(block_lines)
            blocks.append((name, block_text))
            i = j
        else:
            i += 1
    
    return blocks


def extract_preprocessor_directives(lines):
    """Extract all preprocessor directives from the file."""
    directives = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith('#'):
            directives.append(line)
    return directives


def extract_prototypes(blocks):
    """Extract function prototypes from function definitions for forward declarations."""
    prototypes = []
    for name, block in blocks:
        lines = block.split('\n')
        if not lines:
            continue
        first_line = lines[0].strip()
        # Check if this looks like a function (has parentheses)
        if '(' in first_line and '{' in block:
            # Extract the signature up to the opening brace
            sig = first_line
            if '{' in sig:
                sig = sig[:sig.index('{')].strip()
            if sig and '(' in sig and not sig.endswith('{'):
                prototypes.append(sig.rstrip(';') + ';')
    return prototypes


def merge_c_files(input_files, output_file):
    """Merge multiple C files into one, deduplicating by name."""
    
    seen_names = set()
    all_blocks = []
    all_prototypes = []
    all_directives = []
    seen_directives = set()
    
    for filepath in input_files:
        with open(filepath, 'r') as f:
            content = f.read()
        lines = content.split('\n')
        
        # Extract preprocessor directives
        directives = extract_preprocessor_directives(lines)
        for d in directives:
            if d not in seen_directives:
                all_directives.append(d)
                seen_directives.add(d)
        
        # Extract top-level blocks
        blocks = extract_top_level_blocks(lines)
        for name, block in blocks:
            if name and name not in seen_names:
                seen_names.add(name)
                all_blocks.append((name, block))
    
    # Collect all prototypes first for forward declarations
    for name, block in all_blocks:
        protos = extract_prototypes([(name, block)])
        all_prototypes.extend(protos)
    
    # Write output
    with open(output_file, 'w') as f:
        f.write("/* Merged by merge-c.sh */\n\n")
        
        # Write preprocessor directives
        for d in all_directives:
            f.write(d + '\n')
        if all_directives:
            f.write('\n')
        
        # Write all prototypes first (forward declarations)
        seen_protos = set()
        for proto in all_prototypes:
            if proto not in seen_protos:
                f.write(proto + '\n')
                seen_protos.add(proto)
        if all_prototypes:
            f.write('\n')
        
        # Write all blocks
        for name, block in all_blocks:
            f.write(block + '\n\n')
    
    print(f"Merged {len(input_files)} files into {output_file} ({len(all_blocks)} definitions)")


if __name__ == '__main__':
    args = sys.argv[1:]
    output = args[0]
    inputs = args[1:]
    merge_c_files(inputs, output)
PYTHON_SCRIPT

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

# Merge multiple CIR MLIR files into a single MLIR module by extracting
# top-level operations from each module body and deduplicating by symbol
# name (first wins).
#
# Usage: merge-cir.sh <input1.mlir> [input2.mlir ...] <output.mlir>
#
# The last argument is the output file; all preceding arguments are inputs.

set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "Usage: $0 <input1.mlir> [input2.mlir ...] <output.mlir>" >&2
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

python3 - "$OUTPUT" "${INPUTS[@]}" << 'PYTHON_SCRIPT'
import sys
import re

def extract_module_body(lines):
    """
    Extract the body of an MLIR module. The module wrapper is:
        module {
            ... operations ...
        }
    We return the lines inside the braces.
    """
    in_module = False
    brace_depth = 0
    body_lines = []
    started = False
    
    for line in lines:
        stripped = line.strip()
        if not started:
            # Look for module {
            if 'module' in stripped and '{' in stripped:
                started = True
                # Count braces in this line
                for ch in stripped:
                    if ch == '{':
                        brace_depth += 1
                if brace_depth == 0:
                    # Single-line module (unlikely but handle it)
                    return []
                continue
        else:
            for ch in stripped:
                if ch == '{':
                    brace_depth += 1
                elif ch == '}':
                    brace_depth -= 1
            if brace_depth == 0:
                # End of module
                break
            body_lines.append(line)
    
    return body_lines


def extract_top_level_ops(body_lines):
    """
    Extract top-level operations from an MLIR module body.
    Each operation starts at indentation level 0 (or minimal indentation)
    and continues until the next operation at the same level.
    
    Returns a list of (symbol_name, op_text) tuples.
    """
    ops = []
    current_op_lines = []
    current_symbol = None
    
    # Pattern for cir.func: cir.func @name(...)
    func_pattern = re.compile(r'\s*cir\.func\s+@(\w+)')
    # Pattern for cir.global: cir.global @name(...)
    global_pattern = re.compile(r'\s*cir\.global\s+@(\w+)')
    # Pattern for cir.typealias: cir.typealias @name(...)
    typealias_pattern = re.compile(r'\s*cir\.typealias\s+@(\w+)')
    # Pattern for any @symbol definition
    symbol_pattern = re.compile(r'\s*\w+\.\w+\s+@(\w+)')
    
    for line in body_lines:
        stripped = line.strip()
        
        # Skip empty lines and comments at the start
        if not current_op_lines:
            if not stripped or stripped.startswith('//') or stripped.startswith('#'):
                continue
        
        # Check if this line starts a new top-level operation
        # Top-level ops have minimal indentation (typically 0 or 4 spaces)
        indent = len(line) - len(line.lstrip())
        
        if indent <= 4 and stripped and not stripped.startswith('//') and not stripped.startswith('#'):
            # Try to extract symbol name
            symbol = None
            m = func_pattern.match(line)
            if m:
                symbol = m.group(1)
            else:
                m = global_pattern.match(line)
                if m:
                    symbol = m.group(1)
                else:
                    m = typealias_pattern.match(line)
                    if m:
                        symbol = m.group(1)
                    else:
                        m = symbol_pattern.match(line)
                        if m:
                            symbol = m.group(1)
            
            # If we found a symbol and this looks like a new operation,
            # save the previous operation and start a new one
            if symbol and (current_op_lines and current_symbol is not None):
                ops.append((current_symbol, ''.join(current_op_lines)))
                current_op_lines = []
                current_symbol = None
            
            if symbol:
                current_symbol = symbol
        
        if current_symbol is not None or current_op_lines:
            current_op_lines.append(line)
    
    # Don't forget the last operation
    if current_op_lines and current_symbol is not None:
        ops.append((current_symbol, ''.join(current_op_lines)))
    
    return ops


def merge_mlir_files(input_files, output_file):
    """Merge multiple MLIR files into one, deduplicating by symbol name."""
    
    seen_symbols = set()
    all_ops = []
    
    for filepath in input_files:
        with open(filepath, 'r') as f:
            content = f.read()
        lines = content.split('\n')
        
        body_lines = extract_module_body(lines)
        ops = extract_top_level_ops(body_lines)
        
        for symbol, op_text in ops:
            if symbol and symbol not in seen_symbols:
                seen_symbols.add(symbol)
                all_ops.append((symbol, op_text))
    
    # Write merged module
    with open(output_file, 'w') as f:
        f.write("module {\n")
        for symbol, op_text in all_ops:
            f.write(op_text)
            if not op_text.endswith('\n'):
                f.write('\n')
        f.write("}\n")
    
    print(f"Merged {len(input_files)} MLIR files into {output_file} ({len(all_ops)} operations)")


if __name__ == '__main__':
    args = sys.argv[1:]
    output = args[0]
    inputs = args[1:]
    merge_mlir_files(inputs, output)
PYTHON_SCRIPT

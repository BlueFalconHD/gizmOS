#!/usr/bin/env bash

# Generate .clangd file for project, with absolute include path to kernel/src
# Usage: ./generate_clangd.sh

# Get absolute path to kernel/src
KERNEL_SRC_PATH=$(realpath kernel/src)

# Generate .clangd file
echo "CompileFlags:
    Add:
        - \"-I$KERNEL_SRC_PATH\"" > .clangd

echo "Generated .clangd file with include path to kernel/src"

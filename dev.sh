#!/usr/bin/env fish

# Create a build directory if it doesn't exist
if not test -d build
    mkdir build
end

# Define compiler and assembler
set CC clang
set AS clang
set LD ld.lld

# Compiler and assembler flags
set CFLAGS -target aarch64-none-elf -mcpu=cortex-a57 -ffreestanding
set ASFLAGS -target aarch64-none-elf -mcpu=cortex-a57
set LDFLAGS -EL -o build/kernel.elf -T src/linker.ld

# Find source files dynamically and recursively
set C_SOURCES (find src -type f -name '*.c')
set S_SOURCES (find src -type f -name '*.s')

# Object files
set OBJECTS

echo "Compiling C source files with clang..."
for SRC in $C_SOURCES
    set OBJ build/(basename $SRC .c).o
    echo "Compiling $SRC..."
    $CC $CFLAGS -c $SRC -o $OBJ
    set OBJECTS $OBJECTS $OBJ
end

echo "Assembling assembly files with clang..."
for SRC in $S_SOURCES
    set OBJ build/(basename $SRC .s).o
    echo "Assembling $SRC..."
    $AS $ASFLAGS -c $SRC -o $OBJ
    set OBJECTS $OBJECTS $OBJ
end

echo "Linking with ld.lld..."
$LD $LDFLAGS $OBJECTS

echo "Running in QEMU..."
qemu-system-aarch64 -M virt -cpu cortex-a57 -kernel build/kernel.elf -device ramfb -serial stdio -display cocoa,zoom-to-fit=on

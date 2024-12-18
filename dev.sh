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

# Source files
set SOURCES src/main.c src/uart.c src/memory.c src/string_utils.c src/gear.c src/command.c src/ledger_alloc/ledger.c

# Object files
set OBJECTS

echo "Compiling source files with clang..."
for SRC in $SOURCES
    set OBJ build/(basename $SRC .c).o
    echo "Compiling $SRC..."
    $CC $CFLAGS -c $SRC -o $OBJ
    set OBJECTS $OBJECTS $OBJ
end

echo "Assembling boot.s with clang..."
$AS $ASFLAGS -c src/boot.s -o build/boot.o

# Add boot.o to objects
set OBJECTS $OBJECTS build/boot.o

echo "Linking with ld.lld..."
$LD $LDFLAGS $OBJECTS

echo "Running in QEMU..."
qemu-system-aarch64 -M virt -cpu cortex-a57 -kernel build/kernel.elf -nographic

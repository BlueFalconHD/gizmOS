# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

gizmOS is a minimal RISC-V 64-bit kernel written in C that provides foundational OS capabilities with hardware abstraction and memory management. The project is built using LLVM/Clang with the Limine bootloader and currently supports RISC-V 64-bit architecture.

## Development Commands

### Building and Running

```bash
# Download dependencies (run once)
cd kernel/
./get-deps

# Build the kernel
make

# Build and run in QEMU
make run

# Clean build artifacts
make clean

# Remove all dependencies and build artifacts
make distclean
```

### Debugging

```bash
# Enable debugging by uncommenting DEBUG := 1 in both GNUmakefile and kernel/GNUmakefile
# Then build and run with:
make run

# In another terminal, attach LLDB debugger:
./dbg.sh
```

The kernel automatically runs in debug mode when `DEBUG := 1` is enabled, starting QEMU with `-s -S` flags for GDB/LLDB attachment.

### Testing

Tests are built into the kernel and run during boot when the `TESTS` macro is defined in `kernel/src/main.c`. Available test suites:
- Physical allocator tests (`physical_alloc_test.c`)
- Trap/exception handling tests (`trap_test.c`)

## Architecture Overview

### Memory Management
- **Physical allocator**: Buddy allocator system for physical page management (`physical_alloc.c/h`)
- **Virtual memory**: SV39 paging with page table management (`page_table.c/h`)
- **Memory mapping**: MMIO support and memory layout definitions (`mem_layout.h`)

### Process Management
- **Process control**: Basic process structures and management (`proc.c/h`)
- **Context switching**: Assembly implementations for RISC-V context switching (`swtch.S`)
- **Trap handling**: Comprehensive trap and interrupt framework (`trap_handler.c`, `trap.S`)
- **Kernel processes**: Specialized kernel daemons (`kprocs/` directory)

### Device Support
- **UART**: Serial communication interface
- **Framebuffer**: Graphics output with flanterm terminal emulation
- **PLIC**: Platform-Level Interrupt Controller
- **RTC**: Real-time clock
- **VirtIO**: Keyboard, mouse, and GPU support for virtualized environments

### Core Libraries
- **Formatting**: Printf-like functionality (`lib/fmt.c/h`)
- **String operations**: String manipulation utilities (`lib/str.c/h`)
- **Graphics**: Basic 2D graphics primitives (`lib/gfx.c/h`)
- **Synchronization**: Spinlocks and other primitives (`lib/spinlock.c/h`)
- **IPC**: Mailbox system for inter-process communication (`lib/mailbox.c/h`)

### External Dependencies
- **Limine**: Modern multiprotocol bootloader
- **flanterm**: Framebuffer terminal implementation
- **smoldtb**: Minimal Device Tree Blob parser
- **cc-runtime**: Clang compiler runtime for RISC-V

## File Structure Key Points

- `kernel/src/` - Main kernel source code
- `kernel/src/device/` - Hardware device drivers
- `kernel/src/lib/` - Core kernel libraries and utilities
- `kernel/src/kprocs/` - Kernel process implementations
- `kernel/src/tests/` - Built-in test suites
- `kernel/GNUmakefile` - Kernel build configuration
- `GNUmakefile` - Top-level build and QEMU runner
- `limine.conf` - Bootloader configuration

## Build System Notes

- Target architecture is set to `riscv64` by default
- Clang path is hardcoded to `/opt/homebrew/opt/llvm/bin/clang` (macOS Homebrew)
- Debug builds use `-O0`, release builds use `-O2`
- LLD linker is used with custom linker script (`linker-riscv64.ld`)
- Dependencies are automatically downloaded via `get-deps` script

## Development Guidelines

- The kernel uses a custom context switching mechanism with trapframes
- Process management is currently in early development with basic init process support
- VirtIO drivers are implemented for QEMU virtualization compatibility
- Memory management follows a buddy allocator design for physical pages
- Device tree parsing is used for hardware discovery

## Interaction Guidelines

- Don't try to run the project yourself. Instead tell me to do it and get back to you with the proper logs and information.
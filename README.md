# gizmOS

## A RISC-V 64-bit kernel written in C

gizmOS is a minimal RISC-V 64 kernel that provides foundational OS capabilities with a focus on hardware abstraction and memory management.

### Features

- [x] Hardware interfaces

  - [x] UART for serial communication
  - [x] Framebuffer support with flanterm terminal emulation
  - [x] Real-time clock (RTC)
  - [x] Platform-Level Interrupt Controller (PLIC)

- [x] Memory management

  - [x] Physical page allocator
  - [x] Virtual memory with SV39 paging
  - [x] MMIO mapping

- [x] System facilities

  - [x] Formatted output (printf-like functionality)
  - [x] String manipulation
  - [x] Time functions with sleep capabilities
  - [x] Basic math functions

- [x] Exception and interrupt handling

  - [x] Trap framework
  - [x] External interrupts

- [x] Device Tree Blob (DTB) parsing

  - [x] Initial loading and parsing via smoldtb
  - [x] Hardware detection support

- [x] Testing framework
  - [x] Physical allocator tests
  - [x] Trap/exception tests

### Building

**Prerequisites**

- LLVM/Clang toolchain (with RISC-V support)
- GNU Make
- LLD linker

First, download the dependencies:

```bash
cd kernel/
chmod u+x get-deps
./get-deps
```

> [!NOTE]
> You may need to adjust the compiler path in `kernel/GNUmakefile` to match your system configuration.

To build the kernel:

```bash
make
```

To build the kernel and run it in QEMU:

```bash
make run
```

If you desire to enable debugging features, uncomment the `DEBUG := 1` line in `kernel/GNUmakefile` and `GNUmakefile`. Then, you can use `make run` and also use `./dbg.sh` to automatically attach LLDB to gizmOS and set a breakpoint on the trap handler.

### Project Structure

- `kernel/` - Kernel source code
  - `src/` - Main source files
    - `device/` - Device drivers (UART, framebuffer, console, PLIC, RTC)
    - `dtb/` - Device Tree Blob parsing
    - `lib/` - Core libraries (memory, strings, time, math, formatting)
    - `extern/` - External libraries
      - `flanterm/` - Framebuffer terminal
      - `smoldtb/` - Device tree parser
    - `tests/` - Test suites for various components
    - `*.c/*.h` - Core kernel implementation (memory management, paging, etc.)

### Development

gizmOS is a work in progress. Key areas for future development include:

- Advanced memory allocation (beyond basic page allocation)
- Process management
- Filesystem support
- More comprehensive device drivers
- User-space support

Contributions are welcome!

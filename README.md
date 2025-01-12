# gizmOS
## minimal aarch64 kernel in C

Features:
- [x] PL011 UART (Note: use DTB parsing to get address eventually)
- [x] Framebuffer support (formerly rawfb only, now supports any framebuffer backed by Limine)
- [x] Memory allocation (kinda, not really sure if it works very well)
- [x] Real time clock
- [ ] DTB parsing
  - [x] Initial loading
  - [ ] Parsing
- [ ] Interrupts
- [ ] Timers
- [ ] Keyboard input

### Building

> [!WARNING]
> You need to change the path to the compiler in kernel/GNUmakefile (CC = ...), as it is hardcoded for my system currently.

To build the kernel, run the following command:
```bash
make
```

To build **AND** run the kernel in QEMU, run the following command:
```bash
make run
```

### Development
I have been trying to keep source files organized in the source code. Here is a brief overview of the directories in the project:
- `kernel/` - Contains the kernel source code
  - `src/` - Contains the source files for the kernel
    - `device/` - Contains handling for individual devices. Things like the UART, framebuffer, etc.
    - `dtb/` - Contains DTB parsing code, currently only loads the DTB into memory
    - `flanterm/` - Contains the framebuffer terminal code, https://github.com/mintsuki/flanterm/
    - `font/` - Contains the font used for raw drawing to framebuffer, not used in flanterm
    - `qemu/` - Contains code specific to running the kernel in QEMU
    - `main.c` - The main entry point for the kernel
    - `memory.*` - Memory allocation code
    - `time.*` - Time parsing code, from/to Unix time
    - `string.*` - String manipulation code, like strlen, strcmp, etc.
    - `limine.h` - Limine framebuffer support, automatically downloaded by the Makefile
    - ~~`math.*` - Math functions, like pow, sqrt, etc.~~ Currently commented out, because floating point registers are disabled as of now

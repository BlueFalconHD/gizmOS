# Nuke built-in rules and variables.
MAKEFLAGS += -rR
.SUFFIXES:

# This is the name that our final executable will have.
# Change as needed.
override OUTPUT := kernel

DEBUG := 1

# Target architecture to build for. Default to x86_64.
ARCH := riscv64

# Install prefix; /usr/local is a good, standard default pick.
PREFIX := /usr/local

# Check if the architecture is supported.
ifeq ($(filter $(ARCH),aarch64 loongarch64 riscv64 x86_64),)
    $(error Architecture $(ARCH) not supported)
endif

# User controllable C compiler command.
CC := /opt/homebrew/opt/llvm/bin/clang

# User controllable archiver command.
AR := ar

# User controllable C flags.
# -g: Generate debug information.
# -O2: Optimize for speed.
# -pipe: Use pipes rather than temporary files for communication between the various stages of compilation.


CFLAGS := -g

# if debug is enabled, use -O0, otherwise use -O2
ifeq ($(DEBUG),1)
	CFLAGS += -O0
else
	CFLAGS += -O2
endif

CFLAGS += -pipe -I src

# User controllable C preprocessor flags. We set none by default.
CPPFLAGS := -fno-omit-frame-pointer

ifeq ($(ARCH),x86_64)
    # User controllable nasm flags.
    NASMFLAGS := -F dwarf -g
endif

# User controllable linker flags. We set none by default.
LDFLAGS := -fuse-ld=/opt/homebrew/bin/ld.lld

# Ensure the dependencies have been obtained.
ifneq ($(shell ( test '$(MAKECMDGOALS)' = clean || test '$(MAKECMDGOALS)' = distclean ); echo $$?),0)
    ifeq ($(shell ( ! test -d freestnd-c-hdrs-0bsd || ! test -d cc-runtime || ! test -f src/limine.h ); echo $$?),0)
        $(error Please run the ./get-deps script first)
    endif
endif

# Check if CC is Clang.
override CC_IS_CLANG := $(shell ! $(CC) --version 2>/dev/null | grep 'clang' >/dev/null 2>&1; echo $$?)

GENHDR_DIR := gen-$(ARCH)
GENHDR     := $(GENHDR_DIR)/context_offsets_static.h

# Internal C flags that should not be changed by the user.
override CFLAGS += \
    -Wall \
    -Wextra \
    -std=gnu11 \
    -nostdinc \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-PIC \
    -ffunction-sections \
    -fdata-sections \
    -isysroot /dev/null


# Internal C preprocessor flags that should not be changed by the user.
override CPPFLAGS := \
    -I src \
    -I $(GENHDR_DIR) \
    -isystem freestnd-c-hdrs-0bsd \
    $(CPPFLAGS) \
    -DLIMINE_API_REVISION=2 \
    -MMD \
    -MP \
    -isysroot /dev/null

ifeq ($(ARCH),x86_64)
    # Internal nasm flags that should not be changed by the user.
    override NASMFLAGS += \
        -Wall
endif

# Architecture specific internal flags.
ifeq ($(ARCH),x86_64)
    ifeq ($(CC_IS_CLANG),1)
        override CC += \
            -target x86_64-unknown-none
    endif
    override CFLAGS += \
        -m64 \
        -march=x86-64 \
        -mno-80387 \
        -mno-mmx \
        -mno-sse \
        -mno-sse2 \
        -mno-red-zone \
        -mcmodel=kernel
    override LDFLAGS += \
        -Wl,-m,elf_x86_64
    override NASMFLAGS += \
        -f elf64
endif
ifeq ($(ARCH),aarch64)
    ifeq ($(CC_IS_CLANG),1)
        override CC += \
            -target aarch64-none-elf
    endif
    override LDFLAGS += \
        -Wl,-m,aarch64elf
endif
ifeq ($(ARCH),riscv64)
    ifeq ($(CC_IS_CLANG),1)
        override CC += \
            -target riscv64-unknown-none
        override CFLAGS += \
            -march=rv64imac
    else
        override CFLAGS += \
            -march=rv64imac_zicsr_zifencei
    endif
    override CFLAGS += \
        -mabi=lp64 \
        -mno-relax
    override LDFLAGS += \
        -Wl,-m,elf64lriscv \
        -Wl,--no-relax
endif
ifeq ($(ARCH),loongarch64)
    ifeq ($(CC_IS_CLANG),1)
        override CC += \
            -target loongarch64-unknown-none
    endif
    override CFLAGS += \
        -march=loongarch64 \
        -mabi=lp64s
    override LDFLAGS += \
        -Wl,-m,elf64loongarch \
        -Wl,--no-relax
endif

# Internal linker flags that should not be changed by the user.
override LDFLAGS := -fuse-ld=lld \
    -isysroot /dev/null \
    -Wl,--build-id=none \
    -nostdlib \
    -static \
    -z max-page-size=0x1000 \
    -Wl,--gc-sections \
    -T linker-$(ARCH).ld

# Use "find" to glob all *.c, *.S, *.asm, and *.s files in the tree and obtain the
# object and header dependency file names.
override SRCFILES := $(shell cd src && find -L * -type f | LC_ALL=C sort)
override CFILES := $(filter %.c,$(SRCFILES))
override ASFILES := $(filter %.S,$(SRCFILES))
ifeq ($(ARCH),x86_64)
override NASMFILES := $(filter %.asm,$(SRCFILES))
endif
ifeq ($(ARCH),riscv64)
override RISCV_SFILES := $(filter %.s,$(SRCFILES))
endif
override OBJ := $(addprefix obj-$(ARCH)/,$(CFILES:.c=.c.o) $(ASFILES:.S=.S.o))
ifeq ($(ARCH),x86_64)
override OBJ += $(addprefix obj-$(ARCH)/,$(NASMFILES:.asm=.asm.o))
endif
ifeq ($(ARCH),riscv64)
override OBJ += $(addprefix obj-$(ARCH)/,$(RISCV_SFILES:.s=.s.o))
endif
override HEADER_DEPS := $(addprefix obj-$(ARCH)/,$(CFILES:.c=.c.d) $(ASFILES:.S=.S.d))

# Default target. This must come first, before header dependencies.
.PHONY: all
all: $(GENHDR) bin-$(ARCH)/$(OUTPUT)

# Include header dependencies.
-include $(HEADER_DEPS)

# ---------------------------------------------------------------------------
# Produce a plain-numeric header so assembly can #include it safely.
# ---------------------------------------------------------------------------
# $(GENHDR): src/lib/context_offsets.h src/lib/gizmo.h GNUmakefile
# 	mkdir -p "$(GENHDR_DIR)"
# 	( \
# 		echo '#include <lib/context_offsets.h>'; \
# 		echo '#define GEN(sym) asm volatile("\\n#define " #sym " %0" :: "i"(sym))'; \
# 		echo 'void _gen(void) {'; \
# 		echo '  GEN(CTX_OFF_RA);  GEN(CTX_OFF_SP);  GEN(CTX_OFF_GP);  GEN(CTX_OFF_TP);'; \
# 		echo '  GEN(CTX_OFF_T0);  GEN(CTX_OFF_T1);  GEN(CTX_OFF_T2);  GEN(CTX_OFF_T3);'; \
# 		echo '  GEN(CTX_OFF_T4);  GEN(CTX_OFF_T5);  GEN(CTX_OFF_T6);'; \
# 		echo '  GEN(CTX_OFF_S0);  GEN(CTX_OFF_S1);  GEN(CTX_OFF_S2);  GEN(CTX_OFF_S3);'; \
# 		echo '  GEN(CTX_OFF_S4);  GEN(CTX_OFF_S5);  GEN(CTX_OFF_S6);  GEN(CTX_OFF_S7);'; \
# 		echo '  GEN(CTX_OFF_S8);  GEN(CTX_OFF_S9);  GEN(CTX_OFF_S10); GEN(CTX_OFF_S11);'; \
# 		echo '  GEN(CTX_OFF_A0);  GEN(CTX_OFF_A1);  GEN(CTX_OFF_A2);  GEN(CTX_OFF_A3);'; \
# 		echo '  GEN(CTX_OFF_A4);  GEN(CTX_OFF_A5);  GEN(CTX_OFF_A6);  GEN(CTX_OFF_A7);'; \
# 		echo '  GEN(CTX_OFF_SATP); GEN(CTX_OFF_PC); GEN(CTX_SIZE);'; \
# 		echo '}'; \
# 	) > $(GENHDR_DIR)/gen_offsets.c

# 	$(CC) $(CFLAGS) $(CPPFLAGS) -S $(GENHDR_DIR)/gen_offsets.c -o - \
#     | awk '/^[[:space:]]*#define/ { sub(/^[[:space:]]+/, ""); print }' \
#     > $(GENHDR)

# 	rm $(GENHDR_DIR)/gen_offsets.c

# Link rules for building the C compiler runtime.
cc-runtime-$(ARCH)/cc-runtime.a: GNUmakefile cc-runtime/*
	rm -rf cc-runtime-$(ARCH)
	cp -r cc-runtime cc-runtime-$(ARCH)
	$(MAKE) -C cc-runtime-$(ARCH) -f cc-runtime.mk \
		CC="$(CC)" \
		AR="$(AR)" \
		CFLAGS="$(CFLAGS)" \
		CPPFLAGS='-isystem ../freestnd-c-hdrs-0bsd -DCC_RUNTIME_NO_FLOAT'

# Link rules for the final executable.
bin-$(ARCH)/$(OUTPUT): GNUmakefile linker-$(ARCH).ld $(OBJ) cc-runtime-$(ARCH)/cc-runtime.a
	mkdir -p "$$(dirname $@)"
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) cc-runtime-$(ARCH)/cc-runtime.a -o $@

# Compilation rules for *.c files.
obj-$(ARCH)/%.c.o: src/%.c GNUmakefile
	mkdir -p "$$(dirname $@)"
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Compilation rules for *.S files.
obj-$(ARCH)/%.S.o: src/%.S GNUmakefile
	mkdir -p "$$(dirname $@)"
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

ifeq ($(ARCH),x86_64)
# Compilation rules for *.asm (nasm) files.
obj-$(ARCH)/%.asm.o: src/%.asm GNUmakefile
	mkdir -p "$$(dirname $@)"
	nasm $(NASMFLAGS) $< -o $@
endif

ifeq ($(ARCH),riscv64)
# Compilation rules for *.s files.
obj-$(ARCH)/%.s.o: src/%.s GNUmakefile
	mkdir -p "$$(dirname $@)"
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@
endif

# Remove object files and the final executable.
.PHONY: clean
clean:
	rm -rf bin-$(ARCH) obj-$(ARCH) cc-runtime-$(ARCH)

# Remove everything built and generated including downloaded dependencies.
.PHONY: distclean
distclean:
	rm -rf bin-* obj-* freestnd-c-hdrs-0bsd cc-runtime* src/limine.h

# Install the final built executable to its final on-root location.
.PHONY: install
install: all
	install -d "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)"
	install -m 644 bin-$(ARCH)/$(OUTPUT) "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)/$(OUTPUT)-$(ARCH)"

# Try to undo whatever the "install" target did.
.PHONY: uninstall
uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)/$(OUTPUT)-$(ARCH)"
	-rmdir "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)"

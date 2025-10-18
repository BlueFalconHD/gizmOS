#pragma once

#define RAM_START 0x80000000
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))
#define TRAMPOLINE (MAXVA - 4096)
#define KSTACK_GUARD_PAGES 1
#define KSTACK_PAGES 16
#define KSTACK_STRIDE ((KSTACK_GUARD_PAGES + KSTACK_PAGES) * 4096)
#define KSTACK(p) (TRAMPOLINE - ((p) + 1) * KSTACK_STRIDE)
#define TRAPFRAME (TRAMPOLINE - 4096)

// Reserved per-process user buffer for notifications
// Keep a safe gap from TRAMPOLINE/TRAPFRAME and kernel stack region.
// Place 32 MiB below TRAMPOLINE to avoid KSTACK range (approx 4.5 MiB).
#define NOTIF_BUF_SIZE (64 * 1024)
#define NOTIF_BUF_BASE (TRAMPOLINE - (32 * 1024 * 1024))

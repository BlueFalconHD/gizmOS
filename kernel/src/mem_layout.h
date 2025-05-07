#pragma once

#define RAM_START 0x80000000
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))
#define TRAMPOLINE (MAXVA - 4096)
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*4096)
#define TRAPFRAME (TRAMPOLINE - 4096)

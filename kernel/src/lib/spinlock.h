#pragma once

#include <lib/types.h>

struct spinlock {
  g_bool locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
};

void initlock(struct spinlock *lk, char *name);
g_bool holding(struct spinlock *lk);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);

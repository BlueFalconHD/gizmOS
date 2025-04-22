#include "clint.h"
#include "lib/print.h"
#include <lib/result.h>
#include <physical_alloc.h>

// CLINT register offsets
#define CLINT_MSIP_BASE                                                        \
  0x0000 // Machine software interrupt pending (4 bytes per hart)
#define CLINT_MTIMECMP_BASE 0x4000 // Machine time compare (8 bytes per hart)
#define CLINT_MTIME 0xBFF8         // Machine time (8 bytes)

RESULT_TYPE(clint_t *) make_clint(uint64_t base) {
  clint_t *clint = (clint_t *)alloc_page();
  if (!clint) {
    return RESULT_FAILURE(RESULT_NOMEM);
  }

  // Initialize basic fields
  clint->base = base;
  clint->is_initialized = false;

  return RESULT_SUCCESS(clint);
}

g_bool clint_init(clint_t *clint) {
  if (!clint) {
    return false;
  }

  if (clint->is_initialized) {
    return true;
  }

  clint->is_initialized = true;
  return true;
}

uint32_t clint_get_msip(clint_t *clint) {
  if (!clint || !clint->is_initialized) {
    return 0;
  }

  volatile uint32_t *msip =
      (volatile uint32_t *)(clint->base + CLINT_MSIP_BASE);
  return *msip;
}

g_bool clint_set_msip(clint_t *clint, uint32_t value) {
  if (!clint || !clint->is_initialized) {
    return false;
  }

  volatile uint32_t *msip =
      (volatile uint32_t *)(clint->base + CLINT_MSIP_BASE);
  *msip = value;
  return true;
}

uint64_t clint_get_mtime(clint_t *clint) {
  if (!clint || !clint->is_initialized) {
    return 0;
  }

  volatile uint64_t *mtime = (volatile uint64_t *)(clint->base + CLINT_MTIME);
  return *mtime;
}

g_bool clint_set_mtimecmp(clint_t *clint, uint64_t value) {
  if (!clint || !clint->is_initialized) {
    return false;
  }

  volatile uint64_t *mtimecmp =
      (volatile uint64_t *)(clint->base + CLINT_MTIMECMP_BASE);
  *mtimecmp = value;
  return true;
}

uint64_t clint_get_mtimecmp(clint_t *clint) {
  if (!clint || !clint->is_initialized) {
    return 0;
  }

  volatile uint64_t *mtimecmp =
      (volatile uint64_t *)(clint->base + CLINT_MTIMECMP_BASE);
  return *mtimecmp;
}

#pragma once

typedef enum {
  EARLY_INIT_SUCCESS,
  EARLY_INIT_FAIL_CREATE_TABLE,
  EARLY_INIT_FAIL_KERNEL_MAP,
  EARLY_INIT_FAIL_FULL_RAM_MAP,
} early_init_status;

#define EARLY_TEXT __attribute__((section(".text.early"), used, noinline))
#define EARLY_RODATA __attribute__((section(".rodata.early"), used))

early_init_status early_init();

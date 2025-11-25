// Unified descriptor core for processes
#pragma once

#include <lib/types.h>
struct proc;

typedef enum {
  DESC_DEV_UART = 0,
  DESC_DEV_CONSOLE = 1,
  DESC_OBJ_DIR = 2,
  DESC_OBJ_FILE = 3,
} desc_type_t;

#define DESC_RIGHT_R        0x01
#define DESC_RIGHT_W        0x02
#define DESC_RIGHT_ENUM     0x04
#define DESC_RIGHT_ATTR_R   0x08
#define DESC_RIGHT_ATTR_W   0x10
#define DESC_RIGHT_LINK     0x20

typedef struct descriptor {
  uint8_t  type;
  uint8_t  rights;
  uint16_t generation;
  uint32_t flags;
  uint64_t object_id;
  uint64_t offset;
} descriptor_t;

#define PROC_MAX_DESC 128

// Access helpers
int  desc_alloc(struct proc *p, descriptor_t **out, int *index);
void desc_free(struct proc *p, int index);
int  desc_get(struct proc *p, int index, descriptor_t **out); // no generation yet



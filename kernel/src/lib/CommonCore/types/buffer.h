#pragma once

#include "core.h"

typedef struct {
  Size length;        // Length of the buffer in bytes
  u8 *data;           // Pointer to the buffer data
  Bool was_allocated; // Indicates if the buffer was dynamically allocated.
                      // Stack buffers should set this to false.
} Buffer;

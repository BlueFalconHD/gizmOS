#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "buffer.h"
#include "core.h"

/**
 * Object - Generic object container for dynamic typing.
 *
 * This structure represents a dynamically typed object that can hold
 * various types of data. It includes a type identifier and a union
 * to store the actual data.
 */

typedef enum {
  OBJECT_TYPE_NIL, // Represents a null or undefined value
  OBJECT_TYPE_U64, // Unsigned 64-bit integer
  OBJECT_TYPE_I64, // Signed 64-bit integer
  OBJECT_TYPE_U32, // Unsigned 32-bit integer
  OBJECT_TYPE_I32, // Signed 32-bit integer
  OBJECT_TYPE_U16, // Unsigned 16-bit integer
  OBJECT_TYPE_I16, // Signed 16-bit integer
  OBJECT_TYPE_U8,  // Unsigned 8-bit integer
  OBJECT_TYPE_I8,  // Signed 8-bit integer
  OBJECT_TYPE_PTR  // Opaque pointer type
} ObjectType;

typedef struct {
  ObjectType type; // Type identifier for the object
  union {
    u64 u64_value; // Unsigned 64-bit integer value
    i64 i64_value; // Signed 64-bit integer value
    u32 u32_value; // Unsigned 32-bit integer value
    i32 i32_value; // Signed 32-bit integer value
    u16 u16_value; // Unsigned 16-bit integer value
    i16 i16_value; // Signed 16-bit integer value
    u8 u8_value;   // Unsigned 8-bit integer value
    i8 i8_value;   // Signed 8-bit integer value
    Ptr ptr_value; // Opaque pointer value
  } data;          // Union to hold the actual data
} Object;

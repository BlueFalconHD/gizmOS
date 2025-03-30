#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t g_usize;
typedef int64_t g_ssize;
typedef uint8_t g_byte;
typedef uint16_t g_word;
typedef uint32_t g_dword;
typedef uint64_t g_qword;
typedef uint8_t g_bool;
typedef uint8_t g_char;

typedef g_byte g_allocator_id; // Allocator ID for tracking memory allocation

typedef struct {
  g_usize length;           // Length of the string
  g_allocator_id allocator; // Allocator ID of string data
  g_char *str;              // Pointer to the string data
} g_string;

typedef struct {
  g_usize length;           // Length of the buffer
  g_allocator_id allocator; // Allocator ID of buffer data. 0 if not allocated.
  g_byte *data;             // Pointer to the buffer data
} g_buffer;

typedef union {
  g_byte byte;
  g_word word;
  g_dword dword;
  g_qword qword;
  g_usize usize;   // Generic size type
  g_ssize ssize;   // Generic signed size type
  g_bool boolean;  // Generic boolean type
  g_string string; // Generic string type
  g_buffer buffer; // Generic buffer type
} g_any_value;

typedef enum {
  G_TYPE_BYTE,
  G_TYPE_WORD,
  G_TYPE_DWORD,
  G_TYPE_QWORD,
  G_TYPE_USIZE,
  G_TYPE_SSIZE,
  G_TYPE_BOOL,
  G_TYPE_STRING,
  G_TYPE_BUFFER
} g_any_type;

typedef struct {
  g_any_type type;   // Type of the value
  g_any_value value; // The actual value
} g_any;

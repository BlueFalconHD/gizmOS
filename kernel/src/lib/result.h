#pragma once

#include <lib/panic.h>
#include <stdint.h>

typedef enum {
  RESULT_OK = 0,
  RESULT_ERROR,
  RESULT_NOMEM,
  RESULT_NOENT,
  RESULT_INVALID,
  RESULT_BUSY,
  RESULT_NOTIMPL,
} result_code_t;

typedef struct {
  result_code_t code;
  union {
    void *ptr;
    uint64_t value;
  } data;
} result_t;

#define RESULT_SUCCESS(val)                                                    \
  ((result_t){.code = RESULT_OK, .data.value = (uint64_t)(val)})
#define RESULT_FAILURE(err) ((result_t){.code = (err), .data.value = 0})

static inline bool result_is_ok(result_t result) {
  return result.code == RESULT_OK;
}

static inline uint64_t result_unwrap(result_t result) {
  if (!result_is_ok(result)) {
    panic("Attempted to unwrap failed result");
  }
  return result.data.value;
}

// add inline comment after result_t type to show actual type
#define RESULT_TYPE(T) result_t /* T */

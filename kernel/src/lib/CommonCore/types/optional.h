#pragma once

#include "../utils.h"
#include "core.h"
#include <lib/panic.h>

/**
 * Optionals - Represents a value that may or may not be present.
 *
 * Optionals can be unwrapped to access the underlying value, but attempting to
 * unwrap an empty optional will result in a runtime error.
 */

typedef struct {
  Bool some;
  Ptr value;
} Optional;

/**
 * Creates an empty optional.
 *
 * @return An Optional representing no value.
 */
INLINED Optional None() { return (Optional){.some = false, .value = nil}; }

/**
 * Creates an optional containing a value.
 *
 * @param value Pointer to the value to be stored in the optional.
 * @return An Optional containing the provided value.
 */
INLINED Optional Some(void *value) {
  return (Optional){.some = true, .value = value};
}

/**
 * Unwraps the optional to access the underlying value.
 *
 * @param opt The Optional to unwrap.
 * @return Pointer to the value contained in the optional.
 * @note This function will panic if the optional is empty (None).
 */
INLINED Ptr _Unwrap(Optional opt) {
  if (!opt.some) {
    panic("Attempted to unwrap a None optional");
  }
  return opt.value;
}

#define Unwrap(opt, type) (*(type *)_Unwrap(opt))

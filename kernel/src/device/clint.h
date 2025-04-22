#pragma once

#include <lib/result.h>
#include <lib/types.h>

/**
 * CLINT device structure.
 * This structure holds the base address of the CLINT device and a flag
 * indicating whether the device has been initialized.
 */
typedef struct {
  uint64_t base;
  g_bool is_initialized;
} clint_t;

/**
 * Creates a new CLINT device.
 * @param base The base address of the CLINT device.
 * @return A result_t that can safely be cast to a clint_t pointer if
 * successful.
 */
RESULT_TYPE(clint_t *) make_clint(uint64_t base);

/**
 * Initializes the CLINT device.
 * @param clint The CLINT device to initialize.
 * @return A g_bool indicating success or failure.
 */
g_bool clint_init(clint_t *clint);

/**
 * Gets the msip register
 *
 * @param clint The CLINT device.
 */
uint32_t clint_get_msip(clint_t *clint);

/**
 * Sets the msip register
 *
 * @param clint The CLINT device.
 * @param value The value to set.
 */
g_bool clint_set_msip(clint_t *clint, uint32_t value);

/**
 * Gets the current machine time value.
 * This is a monotonically increasing counter.
 *
 * @param clint The CLINT device.
 * @return The current time value.
 */
uint64_t clint_get_mtime(clint_t *clint);

/**
 * Sets the mtimecmp register.
 * When mtime >= mtimecmp, a timer interrupt is generated.
 *
 * @param clint The CLINT device.
 * @param value The compare value to set.
 * @return A g_bool indicating success or failure.
 */
g_bool clint_set_mtimecmp(clint_t *clint, uint64_t value);

/**
 * Gets the mtimecmp register value.
 *
 * @param clint The CLINT device.
 * @return The current compare value.
 */
uint64_t clint_get_mtimecmp(clint_t *clint);

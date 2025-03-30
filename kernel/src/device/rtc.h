#pragma once

#include "lib/result.h"
#include <lib/types.h>
#include <stdint.h>

/**
 * RTC device structure.
 * This structure holds the base address of the RTC device and a flag
 * indicating whether the device has been initialized.
 */
typedef struct {
  uint64_t base;
  g_bool is_initialized;
} rtc_t;

/**
 * Creates a new RTC device.
 * @param base The base address of the UART device.
 * @return A result_t that can safely be cast to a rtc_t pointer if successful.
 */
RESULT_TYPE(*rtc_t) make_rtc(uint64_t base);

/**
 * Initializes the UART device.
 * @param uart The UART device to initialize.
 * @return A result_t indicating success or failure.
 */
g_bool rtc_init(rtc_t *uart);

/**
 * Gets the current time from the RTC device.
 * @param rtc The RTC device to use.
 * @return The current time in seconds since the epoch.
 */
uint64_t rtc_get_time(rtc_t *rtc);

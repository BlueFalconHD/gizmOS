#pragma once

#include "lib/result.h"
#include <lib/types.h>
#include <stdint.h>

/**
 * UART device structure.
 * This structure holds the base address of the UART device and a flag
 * indicating whether the device has been initialized.
 */
typedef struct {
  uint64_t base;
  g_bool is_initialized;
} uart_t;

/**
 * Creates a new UART device.
 * @param base The base address of the UART device.
 * @return A result_t that can safely be cast to a uart_t pointer if successful.
 */
RESULT_TYPE(*uart_t) make_uart(uint64_t base);

/**
 * Initializes the UART device.
 * @param uart The UART device to initialize.
 * @return A result_t indicating success or failure.
 */
g_bool uart_init(uart_t *uart);

void uart_putc(uart_t *uart, g_char c);
g_char uart_getc(uart_t *uart);
void uart_puts(uart_t *uart, const char *);
void uart_enable_interrupts(uart_t *uart);
void uart_disable_interrupts(uart_t *uart);

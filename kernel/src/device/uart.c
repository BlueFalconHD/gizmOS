#include "uart.h"
#include "lib/result.h"
#include "lib/types.h"
#include "physical_alloc.h"
#include <lib/str.h>

#define LINE_STATUS_REGISTER 0x5
#define LINE_CONTROL_REGISTER 0x3
#define FIFO_CONTROL_REGISTER 0x2
#define INTERRUPT_ENABLE_REGISTER 0x1
#define LINE_STATUS_DATA_READY 0x1

/**
 * Creates a new UART device.
 * @param base The base address of the UART device.
 * @return A result_t that can safely be cast to a uart_t pointer if successful.
 */
RESULT_TYPE(*uart_t) make_uart(uint64_t base) {
  uart_t *uart = (uart_t *)alloc_page();
  if (!uart) {
    return RESULT_FAILURE(RESULT_NOMEM);
  }
  uart->base = base;
  uart->is_initialized = false;

  return RESULT_SUCCESS(uart);
}

/**
 * Initializes the UART device.
 * @param uart The UART device to initialize.
 * @return A result_t indicating success or failure.
 */
g_bool uart_init(uart_t *uart) {
  if (!uart) {
    return false;
  }

  if (uart->is_initialized) {
    return true;
  }

  volatile uint8_t *u = (volatile uint8_t *)uart->base;
  u[LINE_CONTROL_REGISTER] = 0x3;
  u[FIFO_CONTROL_REGISTER] = 0x1;
  u[INTERRUPT_ENABLE_REGISTER] = 0x1;
  uart->is_initialized = true;
  return true;
}

void uart_putc(uart_t *uart, g_char c) {
  if (!uart->is_initialized) {
    return;
  }

  volatile uint8_t *u = (volatile uint8_t *)uart->base;
  u[0] = c;
}

void uart_puts(uart_t *uart, const char *s) {
  if (!s) {
    return;
  }

  if (!uart->is_initialized) {
    return;
  }

  while (*s) {
    uart_putc(uart, *s++);
  }
}

unsigned char uart_getc(uart_t *uart) {
  if (!uart->is_initialized) {
    return 0;
  }

  volatile uint8_t *u = (volatile uint8_t *)uart->base;

  if (!(u[LINE_STATUS_REGISTER] & LINE_STATUS_DATA_READY)) {
    return 0;
  }

  return u[0];
}

void uart_enable_interrupts(uart_t *uart) {
  if (!uart->is_initialized) {
    return;
  }

  volatile uint8_t *u = (volatile uint8_t *)uart->base;
  u[INTERRUPT_ENABLE_REGISTER] = 0x1;
}

void uart_disable_interrupts(uart_t *uart) {
  if (!uart->is_initialized) {
    return;
  }

  volatile uint8_t *u = (volatile uint8_t *)uart->base;
  u[INTERRUPT_ENABLE_REGISTER] = 0x0;
}

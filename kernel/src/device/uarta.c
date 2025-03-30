#include "uarta.h"
#include <lib/device.h>
#include <lib/result.h>
#include <lib/str.h>
#include <stdint.h>

// UART registers and bits
#define UART_ADDRESS 0x10000000
#define LINE_STATUS_REGISTER 0x5
#define LINE_CONTROL_REGISTER 0x3
#define FIFO_CONTROL_REGISTER 0x2
#define INTERRUPT_ENABLE_REGISTER 0x1
#define LINE_STATUS_DATA_READY 0x1

// Forward declarations for device operations
static result_t uart_init(device_t *dev);
static result_t uart_shutdown(device_t *dev);
static void uart_putc(char c);
static void uart_puts(const char *s);
static char uart_getc(void);

// Private state for UART
typedef struct {
  uint64_t uart_base;
} uart_private_t;

// The UART device operations
static uart_device_t uart_ops = {
    .putc = uart_putc, .puts = uart_puts, .getc = uart_getc};

// Global UART private data
static uart_private_t uart_private = {.uart_base = 0};

// The device structure
device_t uart_device = {.name = "uart0",
                        .type = DEVICE_UART,
                        .private_data = &uart_private,
                        .initialized = false,
                        .init = uart_init,
                        .shutdown = uart_shutdown,
                        .uart = &uart_ops};

// Implementation of device operations
static result_t uart_init(device_t *dev) {
  if (dev->initialized) {
    return RESULT_SUCCESS(0);
  }

  uart_private_t *priv = (uart_private_t *)dev->private_data;

  // Set UART base address
  priv->uart_base = UART_ADDRESS;

  // Initialize the UART hardware
  volatile uint8_t *uart = (volatile uint8_t *)priv->uart_base;

  // Set 8-bit word length
  uart[LINE_CONTROL_REGISTER] = 0x3;

  // Enable FIFOs
  uart[FIFO_CONTROL_REGISTER] = 0x1;

  // Enable receiver interrupts
  uart[INTERRUPT_ENABLE_REGISTER] = 0x1;

  dev->initialized = true;
  return RESULT_SUCCESS(0);
}

static result_t uart_shutdown(device_t *dev) {
  if (!dev->initialized) {
    return RESULT_SUCCESS(0);
  }

  dev->initialized = false;
  return RESULT_SUCCESS(0);
}

static void uart_putc(char c) {
  uart_private_t *priv = &uart_private;
  if (!priv->uart_base) {
    return;
  }

  volatile uint8_t *uart = (volatile uint8_t *)priv->uart_base;
  uart[0] = c;
}

static void uart_puts(const char *s) {
  if (!s) {
    return;
  }

  while (*s) {
    uart_putc(*s++);
  }
}

static char uart_getc(void) {
  uart_private_t *priv = &uart_private;
  if (!priv->uart_base) {
    return 0;
  }

  volatile uint8_t *uart = (volatile uint8_t *)priv->uart_base;

  // Check if data is ready
  if (!(uart[LINE_STATUS_REGISTER] & LINE_STATUS_DATA_READY)) {
    return 0;
  }

  // Read the character
  return uart[0];
}

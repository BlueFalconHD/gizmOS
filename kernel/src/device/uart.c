#include "uart.h"
#include <device/term.h>
#include <lib/str.h>

struct pl011_uart *uart;

void uartInit(void *addr) {
  uart = (struct pl011_uart *)addr;

  // Ensure UART is disabled
  uart->UARTCR = 0x0;

  // Set UART 0 Registers
  uart->UARTECR = 0x0; // Clear the recieve status (i.e. error) register
  uart->UARTLCR_H = 0x0 | PL011_LCR_WORD_LENGTH_8 | PL011_LCR_FIFO_DISABLE |
                    PL011_LCR_ONE_STOP_BIT | PL011_LCR_PARITY_DISABLE |
                    PL011_LCR_BREAK_DISABLE;

  uart->UARTIBRD = PL011_IBRD_DIV_38400;
  uart->UARTFBRD = PL011_FBRD_DIV_38400;

  uart->UARTIMSC = 0x0;                   // Mask out all UART interrupts
  uart->UARTICR = PL011_ICR_CLR_ALL_IRQS; // Clear interrupts

  uart->UARTCR =
      0x0 | PL011_CR_UART_ENABLE | PL011_CR_TX_ENABLE | PL011_CR_RX_ENABLE;

  return;
}

void uart_putc(char c) {
  // Wait until FIFO or TX register has space
  while ((uart->UARTFR & PL011_FR_TXFF_FLAG) != 0x0) {
    term_puts("Waiting for FIFO or TX register to have space\n");
  }

  // Write packet into FIFO/tx register
  uart->UARTDR = c;

  // Model requires us to manually send a carriage return
  if ((char)c == '\n') {
    while ((uart->UARTFR & PL011_FR_TXFF_FLAG) != 0x0) {
    }
    uart->UARTDR = '\r';
  }
}

void uart_puts(const char *s) {
  while (*s) {
    term_puts("Sending character: ");
    term_putc(*s);
    uart_putc(*s++);
  }
}

char uart_getc(void) {
  // Wait until FIFO has data
  while ((uart->UARTFR & PL011_FR_RXFF_FLAG) != 0x0) {
  }

  return uart->UARTDR;
}

// Outputs an unsigned integer using uart_puts
void uart_putnbr(unsigned int value) {
  char buffer[11]; // Maximum length for 32-bit unsigned int
  strfuint(value, buffer);
  uart_puts(buffer);
}

// Outputs a pointer address in hexadecimal using uart_puts
void uart_putptr(void *ptr) {
  uintptr_t addr = (uintptr_t)ptr;
  uart_puts("0x");
  char buffer[2 * sizeof(uintptr_t) + 1];
  hexstrfuint(addr, buffer);
  uart_puts(buffer);
}

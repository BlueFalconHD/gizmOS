#ifndef UART_H
#define UART_H

#include <stdint.h>

void uartInit(void *addr);

void uart_putc(char c);
void uart_puts(const char *s);
char uart_getc(void);

void uart_putptr(void *ptr);
void uart_putnbr(unsigned int value);

// void uartSendString(const char*);

// Useful defines for control/status registes
#define PL011_LCR_WORD_LENGTH_8 (0x60)
#define PL011_LCR_WORD_LENGTH_7 (0x40)
#define PL011_LCR_WORD_LENGTH_6 (0x20)
#define PL011_LCR_WORD_LENGTH_5 (0x00)

#define PL011_LCR_FIFO_ENABLE (0x10)
#define PL011_LCR_FIFO_DISABLE (0x00)

#define PL011_LCR_TWO_STOP_BITS (0x08)
#define PL011_LCR_ONE_STOP_BIT (0x00)

#define PL011_LCR_PARITY_ENABLE (0x02)
#define PL011_LCR_PARITY_DISABLE (0x00)

#define PL011_LCR_BREAK_ENABLE (0x01)
#define PL011_LCR_BREAK_DISABLE (0x00)

#define PL011_IBRD_DIV_38400 (0x27)
#define PL011_FBRD_DIV_38400 (0x09)

#define PL011_ICR_CLR_ALL_IRQS (0x07FF)

#define PL011_FR_TXFF_FLAG (0x20)
#define PL011_FR_RXFF_FLAG (0x40)

#define PL011_CR_UART_ENABLE (0x01)
#define PL011_CR_TX_ENABLE (0x0100)
#define PL011_CR_RX_ENABLE (0x0200)

struct pl011_uart {
  volatile unsigned int UARTDR;           // +0x00
  volatile unsigned int UARTECR;          // +0x04
  const volatile unsigned int unused0[4]; // +0x08 to +0x14 reserved
  const volatile unsigned int UARTFR;     // +0x18 - RO
  const volatile unsigned int unused1;    // +0x1C reserved
  volatile unsigned int UARTILPR;         // +0x20
  volatile unsigned int UARTIBRD;         // +0x24
  volatile unsigned int UARTFBRD;         // +0x28
  volatile unsigned int UARTLCR_H;        // +0x2C
  volatile unsigned int UARTCR;           // +0x30
  volatile unsigned int UARTIFLS;         // +0x34
  volatile unsigned int UARTIMSC;         // +0x38
  const volatile unsigned int UARTRIS;    // +0x3C - RO
  const volatile unsigned int UARTMIS;    // +0x40 - RO
  volatile unsigned int UARTICR;          // +0x44 - WO
  volatile unsigned int UARTDMACR;        // +0x48
};

#endif /* UART_H */

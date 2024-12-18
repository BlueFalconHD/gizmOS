#ifndef UART_H
#define UART_H

#include <stdint.h>

/* UART0 registers */
#define UART0_BASE 0x09000000
#define UART0_DR   ((volatile unsigned int *)(UART0_BASE + 0x00))   /* Data Register */
#define UART0_FR   ((volatile unsigned int *)(UART0_BASE + 0x18))   /* Flag Register */
#define UARTFR_TXFF (1 << 5)    /* Transmit FIFO Full */
#define UARTFR_RXFE (1 << 4)    /* Receive FIFO Empty */

void uart_putc(char c);
void uart_puts(const char *s);
char uart_getc(void);

#endif /* UART_H */

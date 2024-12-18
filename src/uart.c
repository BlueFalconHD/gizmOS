#include "uart.h"

void uart_putc(char c)
{
    /* Wait until UART is ready to transmit */
    while (*UART0_FR & UARTFR_TXFF)
        ;
    *UART0_DR = c;
}

void uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n')
            uart_putc('\r');    /* Carriage return for new line */
        uart_putc(*s++);
    }
}

char uart_getc(void)
{
    /* Wait until UART has received something */
    while (*UART0_FR & UARTFR_RXFE)
        ;
    return (char)(*UART0_DR);
}

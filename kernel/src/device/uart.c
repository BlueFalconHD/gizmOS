#include "uart.h"
#include "../string.h"

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

// Outputs an unsigned integer using uart_puts
void uart_putnbr(unsigned int value) {
    char buffer[11]; // Maximum length for 32-bit unsigned int
    utoa(value, buffer);
    uart_puts(buffer);
}

// Outputs a pointer address in hexadecimal using uart_puts
void uart_putptr(void* ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    uart_puts("0x");
    char buffer[2 * sizeof(uintptr_t) + 1];
    itoa_hex(addr, buffer);
    uart_puts(buffer);
}

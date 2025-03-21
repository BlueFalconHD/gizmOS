#include "panic.h"
#include <device/term.h>
#include <lib/ansi.h>
#include <device/framebuffer.h>
#include <font/font_render.h>
#include <hhdm.h>

/* UART0 registers */
#define UART0_PHYS_BASE 0x09000000

// Macros to produce the *virtual* address
#define UART0_BASE ((volatile unsigned int *)(UART0_PHYS_BASE + hhdm_offset))
#define UART0_DR   (UART0_BASE + 0x00/4) // each register is 4 bytes
#define UART0_FR   (UART0_BASE + 0x18/4)
#define UARTFR_TXFF (1 << 5)    /* Transmit FIFO Full */
#define UARTFR_RXFE (1 << 4)    /* Receive FIFO Empty */


void panic(const char *msg) {
    term_puts(ANSI_EFFECT_BOLD);
    term_puts(ANSI_RGB_COLOR("231", "130", "132"));
    term_puts("PANIC: ");
    term_puts(ANSI_EFFECT_RESET);
    term_puts(msg);
    term_puts("\n");

    for (;;) {
        asm ("wfi");
    }
}

void lastresort_uart_init() {
    // Initialize UART for last resort panic
    *UART0_FR = 0x00; // Disable FIFO
    *UART0_DR = 0x00; // Clear data register

    // Set FIFO empty flag
    *UART0_FR = UARTFR_RXFE;
}

void lastresort_uart_putc(char c)
{
    /* Wait until UART is ready to transmit */
    while (*UART0_FR & UARTFR_TXFF)
        ;
    *UART0_DR = c;
}

void lastresort_uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n')
            lastresort_uart_putc('\r');    /* Carriage return for new line */
        lastresort_uart_putc(*s++);
    }
}



void panic_lastresort(const char *msg) {
    lastresort_uart_init();
    lastresort_uart_puts("PANIC: ");
    lastresort_uart_puts(msg);
    lastresort_uart_puts("\n");

    for (;;) {
        asm ("wfi");
    }
}

#pragma once

#include <stdint.h>

#define UART_MIRROR_TO_TERM 0

extern void uart_init();
extern void uart_putc(unsigned char);
extern unsigned char uart_getc(void);
void uart_puts(const char *s);

void uart_mmio_callback(uint64_t addr);

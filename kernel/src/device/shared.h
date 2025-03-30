#pragma once

#include "device/rtc.h"
#include <device/console.h>
#include <device/framebuffer.h>
#include <device/uart.h>
#include <lib/types.h>
#include <stdint.h>

extern uart_t *shared_uart;
extern g_bool shared_uart_initialized;
extern console_t *shared_console;
extern g_bool shared_console_initialized;
extern framebuffer_t *shared_framebuffer;
extern g_bool shared_framebuffer_initialized;
extern rtc_t *shared_rtc;
extern g_bool shared_rtc_initialized;

void set_shared_uart(uart_t *uart);
void set_shared_console(console_t *console);
void set_shared_framebuffer(framebuffer_t *framebuffer);
void set_shared_rtc(rtc_t *rtc);
uint64_t shared_rtc_get_time();

#pragma once

#include "device/virtio/virtio_gpu.h"
#include "device/virtio/virtio_keyboard.h"
#include "device/virtio/virtio_mouse.h"
#include <device/console.h>
#include <device/framebuffer.h>
#include <device/plic.h>
#include <device/rtc.h>
#include <device/uart.h>
#include <device/cursor.h>

#include <lib/macros.h>
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
extern plic_t *shared_plic;
extern g_bool shared_plic_initialized;
extern g_bool shared_clint_initialized;
extern virtio_keyboard_t *shared_virtio_keyboard;
extern g_bool shared_virtio_keyboard_initialized;
extern virtio_mouse_t *shared_virtio_mouse;
extern g_bool shared_virtio_mouse_initialized;
extern cursor_t *shared_cursor;
extern g_bool shared_cursor_initialized;
extern virtio_gpu_t *shared_virtio_gpu;
extern g_bool shared_virtio_gpu_initialized;

void set_shared_uart(uart_t *uart);
void set_shared_console(console_t *console);
void set_shared_framebuffer(framebuffer_t *framebuffer);
void set_shared_rtc(rtc_t *rtc);
void set_shared_plic(plic_t *plic);
void set_shared_virtio_keyboard(virtio_keyboard_t *virtio_keyboard);
void set_shared_virtio_mouse(virtio_mouse_t *virtio_mouse);
void set_shared_cursor(cursor_t *cursor);
void set_shared_virtio_gpu(virtio_gpu_t *virtio_gpu);

G_INLINE uint64_t shared_rtc_get_time(void) {
  if (!shared_rtc_initialized) {
    return 0;
  }
  return rtc_get_time(shared_rtc);
}

G_INLINE void shared_uart_putc(g_char c) {
  if (!shared_uart_initialized) {
    return;
  }
  uart_putc(shared_uart, c);
}

G_INLINE void shared_uart_puts(const char *s) {
  if (!shared_uart_initialized) {
    return;
  }
  uart_puts(shared_uart, s);
}

G_INLINE g_char shared_uart_getc(void) {
  if (!shared_uart_initialized) {
    return 0;
  }
  return uart_getc(shared_uart);
}

// Console functions
G_INLINE void shared_console_putc(g_char c) {
  if (!shared_console_initialized) {
    return;
  }
  console_putc(shared_console, c);
}

G_INLINE void shared_console_puts(const char *s) {
  if (!shared_console_initialized) {
    return;
  }
  console_puts(shared_console, s);
}

G_INLINE g_bool shared_console_init(void) {
  if (!shared_console_initialized) {
    return false;
  }
  return console_init(shared_console);
}

// Framebuffer functions
G_INLINE g_bool shared_framebuffer_init(void) {
  if (!shared_framebuffer_initialized) {
    return false;
  }
  return framebuffer_init(shared_framebuffer);
}

G_INLINE void shared_framebuffer_put_pixel(uint32_t x, uint32_t y,
                                           uint8_t pixel[3]) {
  if (!shared_framebuffer_initialized) {
    return;
  }
  framebuffer_put_pixel(shared_framebuffer, x, y, pixel);
}

// RTC functions
G_INLINE g_bool shared_rtc_init(void) {
  if (!shared_rtc_initialized) {
    return false;
  }
  return rtc_init(shared_rtc);
}

// PLIC functions
G_INLINE g_bool shared_plic_init(void) {
  if (!shared_plic_initialized) {
    return false;
  }
  return plic_init(shared_plic);
}

G_INLINE g_bool shared_plic_set_priority(uint32_t irq, uint32_t priority) {
  if (!shared_plic_initialized) {
    return false;
  }
  return plic_set_priority(shared_plic, irq, priority);
}

G_INLINE g_bool shared_plic_set_threshold(uint32_t hart, uint32_t context,
                                          uint32_t threshold) {
  if (!shared_plic_initialized) {
    return false;
  }
  return plic_set_threshold(shared_plic, hart, context, threshold);
}

G_INLINE g_bool shared_plic_enable_interrupt(uint32_t hart, uint32_t context,
                                             uint32_t irq) {
  if (!shared_plic_initialized) {
    return false;
  }
  return plic_enable_interrupt(shared_plic, hart, context, irq);
}

G_INLINE g_bool shared_plic_disable_interrupt(uint32_t hart, uint32_t context,
                                              uint32_t irq) {
  if (!shared_plic_initialized) {
    return false;
  }
  return plic_disable_interrupt(shared_plic, hart, context, irq);
}

G_INLINE uint32_t shared_plic_claim(uint32_t hart, uint32_t context) {
  if (!shared_plic_initialized) {
    return 0;
  }
  return plic_claim(shared_plic, hart, context);
}

G_INLINE g_bool shared_plic_complete(uint32_t hart, uint32_t context,
                                     uint32_t irq) {
  if (!shared_plic_initialized) {
    return false;
  }
  return plic_complete(shared_plic, hart, context, irq);
}

G_INLINE g_bool shared_plic_is_pending(uint32_t irq) {
  if (!shared_plic_initialized) {
    return false;
  }
  return plic_is_pending(shared_plic, irq);
}

// UTILITIES

G_INLINE g_bool is_shared_char_available() {
  g_bool available = false;

  if (shared_uart_initialized) {
    available = shared_uart->is_initialized;
  }

  if (shared_console_initialized) {
    available = shared_console->is_initialized || available;
  }

  return available;
}

#include "shared.h"
#include "device/plic.h"
#include "device/virtio/virtio_keyboard.h"
#include "device/virtio/virtio_mouse.h"
#include <device/clint.h>
#include <device/console.h>
#include <device/framebuffer.h>
#include <device/rtc.h>
#include <device/uart.h>

#include <device/cursor.h>
#include <lib/macros.h>
#include <lib/types.h>

uart_t *shared_uart = NULL;
g_bool shared_uart_initialized = false;
console_t *shared_console = NULL;
g_bool shared_console_initialized = false;
framebuffer_t *shared_framebuffer = NULL;
g_bool shared_framebuffer_initialized = false;
rtc_t *shared_rtc = NULL;
g_bool shared_rtc_initialized = false;
plic_t *shared_plic = NULL;
g_bool shared_plic_initialized = false;
clint_t *shared_clint = NULL;
g_bool shared_clint_initialized = false;
virtio_keyboard_t *shared_virtio_keyboard = NULL;
g_bool shared_virtio_keyboard_initialized = false;
virtio_mouse_t *shared_virtio_mouse = NULL;
g_bool shared_virtio_mouse_initialized = false;
cursor_t *shared_cursor = NULL;
g_bool shared_cursor_initialized = false;

void set_shared_uart(uart_t *uart) {
  shared_uart = uart;
  shared_uart_initialized = true;
};

void set_shared_console(console_t *console) {
  shared_console = console;
  shared_console_initialized = true;
};

void set_shared_framebuffer(framebuffer_t *framebuffer) {
  shared_framebuffer = framebuffer;
  shared_framebuffer_initialized = true;
};

void set_shared_rtc(rtc_t *rtc) {
  shared_rtc = rtc;
  shared_rtc_initialized = true;
};

void set_shared_plic(plic_t *plic) {
  shared_plic = plic;
  shared_plic_initialized = true;
};

void set_shared_clint(clint_t *clint) {
  shared_clint = clint;
  shared_clint_initialized = true;
};

void set_shared_virtio_keyboard(virtio_keyboard_t *keyboard) {
  shared_virtio_keyboard = keyboard;
  shared_virtio_keyboard_initialized = true;
};

void set_shared_virtio_mouse(virtio_mouse_t *mouse) {
  shared_virtio_mouse = mouse;
  shared_virtio_mouse_initialized = true;
};

void set_shared_cursor(cursor_t *cursor) {
  shared_cursor = cursor;
  shared_cursor_initialized = true;
};

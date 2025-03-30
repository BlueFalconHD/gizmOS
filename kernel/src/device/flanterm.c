#include "flanterm.h"
#include <extern/flanterm/backends/fb.h>
#include <extern/flanterm/flanterm.h>
#include <lib/device.h>
#include <lib/result.h>
#include <lib/str.h>

// Forward declarations for device operations
static result_t flanterm_init(device_t *dev);
static result_t flanterm_shutdown(device_t *dev);
static void flanterm_putc(char c);
static void flanterm_puts(const char *s);

// Private state for flanterm
typedef struct {
  struct flanterm_context *ft_ctx;
  struct limine_framebuffer *fb;
} flanterm_private_t;

// The flanterm device operations
static flanterm_device_t flanterm_ops = {
    .putc = flanterm_putc, .puts = flanterm_puts, .ft_ctx = NULL};

// Global flanterm device instance
static flanterm_private_t flanterm_private = {.ft_ctx = NULL, .fb = NULL};

// The device structure
device_t flanterm_device = {.name = "flanterm",
                            .type = DEVICE_FLANTERM,
                            .private_data = &flanterm_private,
                            .initialized = false,
                            .init = flanterm_init,
                            .shutdown = flanterm_shutdown,
                            .flanterm = &flanterm_ops};

// Implementation of device operations
static result_t flanterm_init(device_t *dev) {
  if (dev->initialized) {
    return RESULT_SUCCESS(0);
  }

  flanterm_private_t *priv = (flanterm_private_t *)dev->private_data;

  // Get framebuffer (assuming get_framebuffer() exists)
  struct limine_framebuffer *fb = get_framebuffer();
  if (!fb) {
    return RESULT_FAILURE(RESULT_NOENT);
  }

  priv->fb = fb;

  // Initialize flanterm context
  priv->ft_ctx = flanterm_fb_init(
      NULL, NULL, fb->address, fb->width, fb->height, fb->pitch,
      fb->red_mask_size, fb->red_mask_shift, fb->green_mask_size,
      fb->green_mask_shift, fb->blue_mask_size, fb->blue_mask_shift, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 1, 0, 0, 0);

  if (!priv->ft_ctx) {
    return RESULT_FAILURE(RESULT_ERROR);
  }

  // Update the flanterm operations with the initialized context
  flanterm_ops.ft_ctx = priv->ft_ctx;

  dev->initialized = true;
  return RESULT_SUCCESS(0);
}

static result_t flanterm_shutdown(device_t *dev) {
  if (!dev->initialized) {
    return RESULT_SUCCESS(0);
  }

  dev->initialized = false;
  return RESULT_SUCCESS(0);
}

static void flanterm_putc(char c) {
  if (!flanterm_ops.ft_ctx) {
    return;
  }

  flanterm_write(flanterm_ops.ft_ctx, &c, 1);
}

static void flanterm_puts(const char *s) {
  if (!flanterm_ops.ft_ctx || !s) {
    return;
  }

  flanterm_write(flanterm_ops.ft_ctx, s, strlen(s));
}

#include "framebuffera.h"

#include <lib/device.h>
#include <lib/result.h>
#include <lib/str.h>

static result_t framebuffer_init(device_t *dev);
static result_t framebuffer_shutdown(device_t *dev);
static void framebuffer_put_pixel(uint32_t x, uint32_t y, uint8_t pixel[3]);

typedef struct {
  struct limine_framebuffer *fb;
} framebuffer_private_t;

static framebuffer_device_t framebuffer_ops = {
    .put_pixel = framebuffer_put_pixel,
};

static framebuffer_private_t framebuffer_private = {.fb = NULL};

device_t framebuffer_device = {
    .name = "framebuffer",
    .type = DEVICE_FB,
    .private_data = &framebuffer_private,
    .initialized = false,
    .init = framebuffer_init,
    .shutdown = framebuffer_shutdown,
    .fb = &framebuffer_ops,
};

static result_t framebuffer_init(device_t *dev) {
  if (dev->initialized) {
    return RESULT_SUCCESS(0);
  }

  framebuffer_private_t *priv = (framebuffer_private_t *)dev->private_data;

  // Get framebuffer (assuming get_framebuffer() exists)
  struct limine_framebuffer *fb = get_framebuffer();
  if (!fb) {
    return RESULT_FAILURE(RESULT_NOENT);
  }

  priv->fb = fb;

  dev->initialized = true;
  return RESULT_SUCCESS(0);
}

static result_t framebuffer_shutdown(device_t *dev) {
  if (!dev->initialized) {
    return RESULT_SUCCESS(0);
  }

  dev->initialized = false;
  return RESULT_SUCCESS(0);
}

static void framebuffer_put_pixel(uint32_t x, uint32_t y, uint8_t pixel[3]) {
  framebuffer_private_t *priv =
      (framebuffer_private_t *)framebuffer_device.private_data;
  struct limine_framebuffer *fb = priv->fb;

  if (x >= fb->width || y >= fb->height) {
    return; // Out of bounds
  }

  uint8_t *pixel_ptr = (uint8_t *)fb->address + (y * fb->pitch) + (x * fb->bpp);
  pixel_ptr[0] = pixel[0]; // Red
  pixel_ptr[1] = pixel[1]; // Green
  pixel_ptr[2] = pixel[2]; // Blue
}

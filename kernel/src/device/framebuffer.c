#include "framebuffer.h"
#include "lib/canary.h"
#include <lib/memory.h>
#include <lib/result.h>
#include <physical_alloc.h>

RESULT_TYPE(framebuffer_t *)
make_framebuffer(struct limine_framebuffer *framebuffer) {
  framebuffer_t *fb = (framebuffer_t *)alloc_page();

  canary_dbg_val((uint64_t)fb);

  if (!fb) {
    return RESULT_FAILURE(RESULT_NOMEM);
  }
  fb->framebuffer = framebuffer;
  return RESULT_SUCCESS(fb);
}

g_bool framebuffer_init(framebuffer_t *fb) {
  if (!fb) {
    return false;
  }

  if (fb->is_initialized) {
    return true;
  }

  struct limine_framebuffer *framebuffer = fb->framebuffer;
  if (!framebuffer) {
    return false;
  }

  return true;
}

void framebuffer_put_pixel(framebuffer_t *fb, uint32_t x, uint32_t y,
                           uint8_t pixel[3]) {
  if (!fb) {
    return;
  }

  struct limine_framebuffer *framebuffer = fb->framebuffer;
  if (!framebuffer) {
    return;
  }

  if (x >= framebuffer->width || y >= framebuffer->height) {
    return;
  }

  uint8_t *pixel_ptr = (uint8_t *)framebuffer->address +
                       (y * framebuffer->pitch) + (x * framebuffer->bpp / 8);
  pixel_ptr[0] = pixel[0];
  pixel_ptr[1] = pixel[1];
  pixel_ptr[2] = pixel[2];
}

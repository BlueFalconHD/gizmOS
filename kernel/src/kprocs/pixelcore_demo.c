#include "pixelcore_demo.h"
#include "device/framebuffer.h"
#include "device/shared.h"
#include "dtb/dtb.h"
#include "img/cursor.h"
#include "lib/macros.h"
#include "lib/print.h"
#include "lib/timer.h"

#include <lib/PixelCore/backbuffer.h>
#include <lib/PixelCore/rect.h>
#include <lib/PixelCore/region.h>
#include <lib/PixelCore/surface.h>
#include <lib/time.h>
#include <proc/scheduler.h>

#define BOXES 120

G_INLINE uint8_t random_byte(void) {
  // Generate a random byte using a simple linear congruential generator
  static uint8_t seed = 0;
  if (seed == 0) {
    seed = (uint8_t)(get_time_in_cycles() & 0xFF);
  }
  seed = (seed * 1103515245 + 12345) & 0xFF; // Simple LCG
  return seed;
}

G_INLINE uint32_t random_uint32(void) {
  // Generate a random 32-bit unsigned integer
  uint32_t r = (random_byte() << 24) | (random_byte() << 16) |
               (random_byte() << 8) | random_byte();
  return r;
}

G_INLINE uint32_t random_color(void) {
  // Generate a random color in ARGB format
  uint8_t r = random_byte();
  uint8_t g = random_byte();
  uint8_t b = random_byte();
  return (0xFFu << 24) | (r << 16) | (g << 8) | b; // ARGB format
}

G_INLINE uint32_t random_color_with_alpha(void) {
  // Generate a random color with alpha in ARGB format
  uint8_t a = random_byte();
  uint8_t r = random_byte();
  uint8_t g = random_byte();
  uint8_t b = random_byte();
  return (a << 24) | (r << 16) | (g << 8) | b; // ARGB format
}

G_INLINE uint32_t random_color_reasonable_opacity(void) {
  // Generate a random color with a reasonable opacity (alpha)
  // alpha must be between 127 and 254
  uint8_t a = random_byte() % 128 + 127;
  uint8_t r = random_byte();
  uint8_t g = random_byte();
  uint8_t b = random_byte();
  return (a << 24) | (r << 16) | (g << 8) | b; // ARGB format
}

G_INLINE uint32_t rand_range(uint32_t min, uint32_t max) {
  // Generate a random number in the range [min, max)
  if (min >= max) {
    return min; // Invalid range, return min
  }
  return min + (random_uint32() % (max - min));
}

void pixelcore_demo(void *arg) {
  const uint64_t frame_us = 10;

  framebuffer_t *fb = (framebuffer_t *)arg;
  if (!fb) {
    panic("Pixelcore demo: framebuffer is null\n");
  }

  printf("Pixelcore demo: framebuffer is at %{type: ptr}\n", PRINT_FLAG_BOTH,
         fb);

  if (!fb->is_initialized) {
    panic("Pixelcore demo: framebuffer is not initialized\n");
  }

  if (!fb->framebuffer || !fb->framebuffer->width || !fb->framebuffer->height) {
    printf("Pixelcore demo: framebuffer dimensions are invalid: "
           "%{type: int}x%{type: int}\n",
           PRINT_FLAG_BOTH, fb->framebuffer->width, fb->framebuffer->height);
    panic("Pixelcore demo: framebuffer dimensions are invalid\n");
  }

  PCBackBuffer *bb =
      PCBackBuffer_create(fb->framebuffer->width, fb->framebuffer->height);
  if (!bb) {
    panic("Pixelcore demo: failed to create backbuffer\n");
  }

  PCRect bg_rect;
  PCRect_init(&bg_rect, 0, 0, fb->framebuffer->width, fb->framebuffer->height);

  PCSurface *bg = PCSurface_create(&bg_rect, 0);
  if (!bg) {
    panic("Pixelcore demo: failed to create background surface\n");
  }

  bg->flags |= PC_SURFACE_OPAQUE | PC_SURFACE_VISIBLE;

  // Fill background with a solid dark color (ARGB: 0xFF181818)
  uint32_t bg_color = 0xFF181818u;
  for (uint32_t y = 0; y < bg->rect.height; ++y) {
    uint32_t *row = bg->pixels + (size_t)y * (size_t)bg->stride;
    for (uint32_t x = 0; x < bg->rect.width; ++x) {
      row[x] = bg_color;
    }
  }

  // Mark full background dirty so the first compose draws it
  PCRect full_bg = {0, 0, bg->rect.width, bg->rect.height};
  PCSurface_mark_dirty(bg, full_bg);

  uint32_t box_w = fb->framebuffer->width / 8;
  if (box_w < 16)
    box_w = 16;
  uint32_t box_h = fb->framebuffer->height / 8;
  if (box_h < 16)
    box_h = 16;

  PCRect box_rects[BOXES];

  for (int i = 0; i < BOXES; ++i) {
    int32_t x = (int32_t)rand_range(box_w, fb->framebuffer->width - box_w);
    int32_t y = (int32_t)rand_range(box_h, fb->framebuffer->height - box_h);
    PCRect_init(&box_rects[i], x, y, box_w, box_h);
  }

  PCSurface *boxes[BOXES];
  for (int i = 0; i < BOXES; ++i) {
    boxes[i] = PCSurface_create(&box_rects[i], 10 + i);
    if (!boxes[i]) {
      panic("Pixelcore demo: failed to create box surface\n");
    }
    // boxes[i]->flags |= PC_SURFACE_OPAQUE | PC_SURFACE_VISIBLE;
    boxes[i]->flags |= PC_SURFACE_VISIBLE;
  }

  uint32_t box_colors[BOXES];

  // generate random colors
  for (int i = 0; i < BOXES; ++i) {
    box_colors[i] = random_color_reasonable_opacity();
  }

  for (int i = 0; i < BOXES; ++i) {
    PCSurface *s = boxes[i];
    uint32_t color = box_colors[i];
    for (uint32_t y = 0; y < s->rect.height; ++y) {
      uint32_t *row = s->pixels + (size_t)y * (size_t)s->stride;
      for (uint32_t x = 0; x < s->rect.width; ++x) {
        row[x] = color;
      }
    }
    PCRect full_box = (PCRect){0, 0, s->rect.width, s->rect.height};
    PCSurface_mark_dirty(s, full_box);
  }

  PCRect cursor_rect;
  PCRect_init(&cursor_rect, 0, 0, IMG_CURSOR_WIDTH, IMG_CURSOR_HEIGHT);

  PCSurface *cursor_surface = PCSurface_create(&cursor_rect, 100);
  if (!cursor_surface) {
    panic("Pixelcore demo: failed to create cursor surface\n");
  }

  // copy data from img_cursor array to the surface
  for (uint32_t y = 0; y < IMG_CURSOR_HEIGHT; ++y) {
    uint32_t *row =
        cursor_surface->pixels + (size_t)y * (size_t)cursor_surface->stride;
    for (uint32_t x = 0; x < IMG_CURSOR_WIDTH; ++x) {
      row[x] = img_cursor[y * IMG_CURSOR_WIDTH + x];
    }
  }

  if (!PCBackBuffer_add_surface(bb, bg)) {
    panic("Pixelcore demo: failed to add background surface to backbuffer\n");
  }

  for (int i = 0; i < BOXES; ++i) {
    if (!PCBackBuffer_add_surface(bb, boxes[i])) {
      panic("Pixelcore demo: failed to add box surface to backbuffer\n");
    }
  }

  if (!PCBackBuffer_add_surface(bb, cursor_surface)) {
    panic("Pixelcore demo: failed to add cursor surface to backbuffer\n");
  }

  // Compose once to initialize the backbuffer
  PCBackBuffer_compose(bb);
  PCBackBuffer_flush_to_framebuffer(bb, fb);

  // Bounce the squares around the screen
  int32_t dxs[BOXES];
  int32_t dys[BOXES];
  int32_t xs[BOXES];
  int32_t ys[BOXES];

  for (int i = 0; i < BOXES; ++i) {
    dxs[i] = (random_byte() % 2 == 0 ? 1 : -1) * rand_range(1, 5);
    dys[i] = (random_byte() % 2 == 0 ? 1 : -1) * rand_range(1, 5);
    xs[i] = boxes[i]->rect.x;
    ys[i] = boxes[i]->rect.y;
  }

  int32_t pcx = 0;
  int32_t pcy = 0;

  while (1) {
    for (int i = 0; i < BOXES; ++i) {
      xs[i] += dxs[i];
      ys[i] += dys[i];

      int32_t max_x = (int32_t)bb->width - (int32_t)boxes[i]->rect.width;
      int32_t max_y = (int32_t)bb->height - (int32_t)boxes[i]->rect.height;

      if (xs[i] < 0) {
        xs[i] = 0;
        dxs[i] = -dxs[i];
      } else if (xs[i] > max_x) {
        xs[i] = max_x;
        dxs[i] = -dxs[i];
      }
      if (ys[i] < 0) {
        ys[i] = 0;
        dys[i] = -dys[i];
      } else if (ys[i] > max_y) {
        ys[i] = max_y;
        dys[i] = -dys[i];
      }

      PCSurface_reposition(boxes[i], xs[i], ys[i]);
      PCBackBuffer_on_surface_moved(bb, boxes[i]);
    }

    if (shared_cursor_initialized) {
      if (shared_cursor->x != pcx || shared_cursor->y != pcy) {
        // Cursor moved, update its position
        pcx = shared_cursor->x;
        pcy = shared_cursor->y;

        // Reposition the cursor surface
        PCSurface_reposition(cursor_surface, pcx, pcy);
        PCBackBuffer_on_surface_moved(bb, cursor_surface);
      }
    }

    PCBackBuffer_compose(bb);
    PCBackBuffer_flush_to_framebuffer(bb, fb);

    // ~60 FPS
    // sleep_us(frame_us);
    yield();
  }
}

#pragma once

#include "../spinlock.h"
#include "../types.h"
#include "region.h"
#include "surface.h"
#include <device/framebuffer.h>
#include <stdint.h>

#define PC_BACKBUFFER_MAX_DIRTY 256
#define PC_BACKBUFFER_MAX_SURFACES 128

typedef struct PCBackBuffer {
  uint32_t *pixels; // 0xAARRGGBB
  uint32_t width;
  uint32_t height;
  uint32_t stride; // in pixels

  PCSurface *surfaces[PC_BACKBUFFER_MAX_SURFACES];
  uint16_t surface_count;

  PCRect dirty_storage[PC_BACKBUFFER_MAX_DIRTY];
  PCRegion dirty; // what needs to be flushed to the front framebuffer

  PCRect collect_storage[PC_BACKBUFFER_MAX_DIRTY];
  PCRegion collect; // input accumulator

  PCRect written_storage[PC_BACKBUFFER_MAX_DIRTY];
  PCRegion written; // what was written this frame after occlusion

  struct spinlock lock;
  uint64_t frame_counter;
} PCBackBuffer;

PCBackBuffer *PCBackBuffer_create(uint32_t width, uint32_t height);
void PCBackBuffer_free(PCBackBuffer *bb);

g_bool PCBackBuffer_add_surface(PCBackBuffer *bb, PCSurface *s);
void PCBackBuffer_remove_surface(PCBackBuffer *bb, PCSurface *s);

void PCBackBuffer_on_surface_reordered(PCBackBuffer *bb, PCSurface *s);
void PCBackBuffer_on_surface_moved(PCBackBuffer *bb, PCSurface *s);
void PCBackBuffer_on_surface_resized(PCBackBuffer *bb, PCSurface *s);

void PCBackBuffer_compose(PCBackBuffer *bb);

void PCBackBuffer_flush(PCBackBuffer *bb, uint32_t *dst_pixels,
                        uint32_t dst_width, uint32_t dst_height,
                        uint32_t dst_stride_pixels);

void PCBackBuffer_flush_to_framebuffer(PCBackBuffer *bb, framebuffer_t *fb);

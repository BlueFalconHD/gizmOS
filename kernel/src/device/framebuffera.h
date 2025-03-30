#pragma once

#include <stddef.h>
#include <stdint.h>

struct device;
struct limine_framebuffer;

typedef struct {
  struct limine_framebuffer *fb;
  void (*put_pixel)(uint32_t x, uint32_t y, uint8_t pixel[3]);
} framebuffer_device_t;

extern struct device framebuffer_device;

#pragma once

#include <stddef.h>

struct flanterm_context;
struct device;

typedef struct {
  struct flanterm_context *ft_ctx;
  void (*putc)(char c);
  void (*puts)(const char *s);
} flanterm_device_t;

struct limine_framebuffer *get_framebuffer(void);

extern struct device flanterm_device;

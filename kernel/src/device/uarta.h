#pragma once

#include <stddef.h>
#include <stdint.h>

struct device;

typedef struct {
  void (*putc)(char c);
  void (*puts)(const char *s);
  char (*getc)(void);
} uart_device_t;

extern struct device uart_device;

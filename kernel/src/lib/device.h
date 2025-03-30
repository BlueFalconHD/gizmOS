#pragma once

#include <lib/result.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct device device_t;

#include <device/flanterm.h>
#include <device/framebuffera.h>
#include <device/uarta.h>

typedef enum {
  DEVICE_FLANTERM = 1,
  DEVICE_UART,
  DEVICE_FB,
  DEVICE_RTC,
} device_type_t;

struct device {
  const char *name;
  device_type_t type;
  void *private_data;
  bool initialized;

  result_t (*init)(device_t *dev);
  result_t (*shutdown)(device_t *dev);

  union {
    flanterm_device_t *flanterm;
    uart_device_t *uart;
    framebuffer_device_t *fb;
  };
};

result_t device_register(device_t *dev);
device_t *device_find_by_name(const char *name);
device_t *device_find_by_type(device_type_t type, int index);
result_t device_init_all(void);

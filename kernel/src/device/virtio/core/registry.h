#pragma once

#include <stdint.h>
#include <lib/types.h>
#include "device.h"

typedef int (*virtio_probe_fn)(virtio_device_t *);

typedef struct virtio_driver {
  uint32_t device_id;
  virtio_probe_fn probe;
} virtio_driver_t;

int  virtio_register_driver(const virtio_driver_t *drv);
void virtio_bus_init_from_dtb(void);
void virtio_bus_init_static(void);



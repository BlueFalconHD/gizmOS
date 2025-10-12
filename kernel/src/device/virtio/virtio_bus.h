#pragma once

#include <lib/types.h>
#include <lib/result.h>
#include "virtio_common.h"

/** Simple registry keyed by IRQ for virtio devices. */
void virtio_bus_init(void);
g_bool virtio_bus_register(virtio_device_t *dev);
virtio_device_t *virtio_bus_get_by_irq(uint32_t irq);
void virtio_bus_handle_irq(uint32_t irq);



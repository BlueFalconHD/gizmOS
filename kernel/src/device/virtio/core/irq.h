#pragma once

#include <stdint.h>
#include "device.h"

typedef void (*virtio_config_changed_cb)(virtio_device_t *dev);

void virtio_shared_isr(uint32_t irq);
void virtio_set_config_changed_callback(virtio_device_t *dev, virtio_config_changed_cb cb);



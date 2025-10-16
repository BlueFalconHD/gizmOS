#pragma once

#include "core/spec.h"
#include "core/mmio.h"
#include "core/device.h"
#include "core/queue.h"
#include "core/irq.h"
#include "core/registry.h"
// Debug logging toggle for VirtIO subsystem
#ifndef VIRTIO_DEBUG
#define VIRTIO_DEBUG 0
#endif
// Call during boot to register built-in drivers into the VirtIO registry
void virtio_register_all_drivers(void);



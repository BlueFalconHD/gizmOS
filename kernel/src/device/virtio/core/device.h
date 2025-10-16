#pragma once

#include <stdint.h>
#include <lib/types.h>
#include <stddef.h>
#include <lib/spinlock.h>
#include "spec.h"

typedef struct virtio_device virtio_device_t;

typedef struct virtq {
  uint16_t            size;
  struct virtq_desc  *desc;
  struct virtq_avail *avail;
  struct virtq_used  *used;
  uint16_t            last_used_idx;
  struct spinlock     lock;
  struct virtio_device *dev;   // parent device
  uint16_t            qidx;    // queue index on device
  void (*on_used)(struct virtq*, void* cookie, size_t bytes, void *user);
  void               *cb_user;
} virtq_t;

struct virtio_device {
  uintptr_t    mmio_base;
  uint32_t     irq;
  g_bool       is_initialized;
  uint64_t     device_features;
  uint64_t     negotiated_features;
  uint32_t     device_id;
  uint16_t     num_queues;
  struct virtq *queues[8];
};

int  virtio_device_init(virtio_device_t *dev, uintptr_t mmio_base, uint32_t irq);
int  virtio_device_negotiate(virtio_device_t *dev, uint64_t wanted, uint64_t required, uint64_t *out);
void virtio_device_reset(virtio_device_t *dev);



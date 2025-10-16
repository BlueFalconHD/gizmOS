#pragma once

#include <stdint.h>
#include <stddef.h>
#include <lib/types.h>
#include <lib/spinlock.h>
#include "device.h"

// Minimal kernel-local iovec for scatter/gather
struct iovec {
  void  *iov_base;
  size_t iov_len;
};

int  virtq_create(virtio_device_t *dev, uint16_t qidx, uint16_t requested_size, struct virtq **out);
int  virtq_submit(struct virtq *q, const struct iovec *out_sg, size_t out_cnt, const struct iovec *in_sg, size_t in_cnt, void *cookie);
int  virtq_kick(virtio_device_t *dev, uint16_t qidx);
void virtq_set_callback(struct virtq *q, void (*on_used)(struct virtq*, void* cookie, size_t bytes, void *user), void *user);
int  virtq_poll_used(struct virtq *q, void **out_cookie, size_t *out_bytes);
void virtq_handle_irq_for_device(virtio_device_t *dev);



#pragma once

#include <lib/result.h>
#include <lib/types.h>
#include "virtio.h"   // descriptor & register definitions

/** Generic VirtIO device wrapper (one per MMIO BAR). */
typedef struct {
  uint64_t base;          /* MMIO base address */
  uint32_t irq;           /* PLIC IRQ line */
  g_bool   is_initialized;
} virtio_device_t;

/** A single virtqueue owned by a driver. */
typedef struct {
  uint16_t           size;           /* number of elements (power‑of‑two)     */
  struct virtq_desc *desc;           /* PAGE‑aligned descriptor table         */
  struct virtq_avail *avail;         /* driver → device ring                  */
  struct virtq_used  *used;          /* device → driver ring                  */
  uint16_t          *free_map;       /* bitmap whether entry is free          */
  uint16_t           last_used_idx;  /* bookkeeping                           */
} virtio_queue_t;

// ---------------------------------------------------------------------------
// Constructors / initialisation helpers
// ---------------------------------------------------------------------------
RESULT_TYPE(virtio_device_t *) make_virtio_device(uint64_t base, uint32_t irq);

/**
 * Reset the device, negotiate feature bits (caller supplies wanted_features),
 * set DRIVER_OK and leave it ready for queue set‑up.
 */
g_bool virtio_device_init(virtio_device_t *dev, uint32_t wanted_features);

/**
 * Convenience helper that allocates & wires *one* queue.
 *   - `elem_size` – size, in bytes, of one element the device will write/read
 *
 * All pages are allocated via `alloc_page()` and left zeroed.
 */
g_bool virtio_queue_setup(virtio_device_t *dev, virtio_queue_t *q,
                          uint16_t qsel, uint16_t size,
                          void       *buffers,
                          uint16_t    elem_size);

static inline uint32_t virtio_mmio_read(virtio_device_t *dev, uint32_t off)
{
    return *(volatile uint32_t *)(dev->base + off);
}

static inline void virtio_mmio_write(virtio_device_t *dev, uint32_t off,
                                     uint32_t val)
{
    *(volatile uint32_t *)(dev->base + off) = val;
}

/** Ack *all* pending interrupts for this device. */
static inline void virtio_ack_irq(virtio_device_t *dev)
{
    uint32_t ist = virtio_mmio_read(dev, VIRTIO_MMIO_INTERRUPT_STATUS);
    virtio_mmio_write(dev, VIRTIO_MMIO_INTERRUPT_ACK, ist);
}

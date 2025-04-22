#include "virtio_common.h"
#include "lib/print.h"
#include <lib/memory.h>
#include <lib/panic.h>
#include <page_table.h>
#include <physical_alloc.h>

RESULT_TYPE(virtio_device_t *)
make_virtio_device(uint64_t base, uint32_t irq) {
  virtio_device_t *dev = (virtio_device_t *)alloc_page();
  if (!dev)
    return RESULT_FAILURE(RESULT_NOMEM);

  dev->base = base;
  dev->irq = irq;
  dev->is_initialized = false;
  return RESULT_SUCCESS(dev);
}

static void set_status(virtio_device_t *dev, uint32_t mask) {
  uint32_t s = virtio_mmio_read(dev, VIRTIO_MMIO_STATUS);
  s |= mask;
  virtio_mmio_write(dev, VIRTIO_MMIO_STATUS, s);
}

static void reset_device(virtio_device_t *dev) {
  virtio_mmio_write(dev, VIRTIO_MMIO_STATUS, 0); // full reset
}

g_bool virtio_device_init(virtio_device_t *dev, uint32_t wanted_features) {
  if (!dev || dev->is_initialized)
    return false;

  // step 1 – sanity‑check header & vendor
  if (virtio_mmio_read(dev, VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
      virtio_mmio_read(dev, VIRTIO_MMIO_VERSION) != 2 ||
      virtio_mmio_read(dev, VIRTIO_MMIO_VENDOR_ID) != 0x554d4551) {
    panic("virtio: invalid header");
  }

  reset_device(dev);
  set_status(dev, VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER);

  virtio_mmio_write(dev, VIRTIO_MMIO_DRIVER_FEATURES, wanted_features);
  set_status(dev, VIRTIO_CONFIG_S_FEATURES_OK);

  if (!(virtio_mmio_read(dev, VIRTIO_MMIO_STATUS) &
        VIRTIO_CONFIG_S_FEATURES_OK)) {
    print("virtio: negotiation failed\n", PRINT_FLAG_BOTH);
    return false; // negotiation failed
  }

  dev->is_initialized = true;
  return true;
}

g_bool virtio_queue_setup(virtio_device_t *dev, virtio_queue_t *q,
                          uint16_t qsel, uint16_t size, void *buffers,
                          uint16_t elem_size) {
  if (!dev || !dev->is_initialized || !q)
    return false;

  // allocate backing pages
  q->size = size;
  q->desc = alloc_page();
  q->avail = alloc_page();
  q->used = alloc_page();
  q->free_map = alloc_page();

  if (!q->desc || !q->avail || !q->used || !q->free_map)
    return false;

  memset(q->desc, 0, PAGE_SIZE);
  memset(q->avail, 0, PAGE_SIZE);
  memset(q->used, 0, PAGE_SIZE);
  memset(q->free_map, 0, PAGE_SIZE);

  // tell the device which queue we mean
  virtio_mmio_write(dev, VIRTIO_MMIO_QUEUE_SEL, qsel);
  virtio_mmio_write(dev, VIRTIO_MMIO_QUEUE_NUM, size);

  uint64_t p_desc = V2P((uint64_t)q->desc);
  uint64_t p_avail = V2P((uint64_t)q->avail);
  uint64_t p_used = V2P((uint64_t)q->used);

  virtio_mmio_write(dev, VIRTIO_MMIO_QUEUE_DESC_LOW, p_desc);
  virtio_mmio_write(dev, VIRTIO_MMIO_QUEUE_DESC_HIGH, p_desc >> 32);
  virtio_mmio_write(dev, VIRTIO_MMIO_DRIVER_DESC_LOW, p_avail);
  virtio_mmio_write(dev, VIRTIO_MMIO_DRIVER_DESC_HIGH, p_avail >> 32);
  virtio_mmio_write(dev, VIRTIO_MMIO_DEVICE_DESC_LOW, p_used);
  virtio_mmio_write(dev, VIRTIO_MMIO_DEVICE_DESC_HIGH, p_used >> 32);

  // prime descriptors so device can write directly into per‑element buffer
  struct virtio_input_event *ev_buf = (struct virtio_input_event *)buffers;
  for (int i = 0; i < size; i++) {
    q->free_map[i] = 1;
    q->desc[i].addr = V2P((uint64_t)&ev_buf[i]);
    q->desc[i].len = elem_size;
    q->desc[i].flags = VRING_DESC_F_WRITE;
    q->avail->ring[i] = i;
  }
  q->avail->idx = size;
  __sync_synchronize();

  virtio_mmio_write(dev, VIRTIO_MMIO_QUEUE_READY, 1);
  virtio_mmio_write(dev, VIRTIO_MMIO_QUEUE_NOTIFY, qsel);

  q->last_used_idx = 0;
  return true;
}

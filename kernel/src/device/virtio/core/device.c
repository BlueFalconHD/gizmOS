#include "device.h"
#include "mmio.h"
#include <lib/panic.h>
#include <lib/print.h>
#include <device/virtio/virtio.h>
#include <lib/print.h>

static inline void set_status(virtio_device_t *dev, uint32_t mask) {
  uint32_t s = virtio_mmio_read32(dev->mmio_base, VIRTIO_MMIO_STATUS);
  virtio_mmio_write32(dev->mmio_base, VIRTIO_MMIO_STATUS, s | mask);
}

void virtio_device_reset(virtio_device_t *dev) {
  virtio_mmio_write32(dev->mmio_base, VIRTIO_MMIO_STATUS, 0);
}

int virtio_device_init(virtio_device_t *dev, uintptr_t mmio_base, uint32_t irq) {
  if (!dev)
    return -1;
  dev->mmio_base = mmio_base;
  dev->irq = irq;
  dev->is_initialized = false;
  dev->device_features = 0;
  dev->negotiated_features = 0;
  dev->device_id = 0;
  dev->num_queues = 0;
  for (size_t i = 0; i < sizeof(dev->queues)/sizeof(dev->queues[0]); i++) dev->queues[i] = NULL;

  uint32_t magic = virtio_mmio_read32(mmio_base, VIRTIO_MMIO_MAGIC_VALUE);
  uint32_t ver = virtio_mmio_read32(mmio_base, VIRTIO_MMIO_VERSION);
  if (magic != 0x74726976u) {
    #if VIRTIO_DEBUG
    printf("virtio: bad magic 0x%{type: hex} at base 0x%{type: hex}\n", PRINT_FLAG_BOTH, (uint64_t)magic, (uint64_t)mmio_base);
    #endif
    return -2;
  }
  if (ver != 2u) {
    #if VIRTIO_DEBUG
    printf("virtio: wrong version %{type: int} (expect 2) at base 0x%{type: hex}\n", PRINT_FLAG_BOTH, ver, (uint64_t)mmio_base);
    #endif
    return -3; // require modern
  }

  dev->device_id = virtio_mmio_read32(mmio_base, VIRTIO_MMIO_DEVICE_ID);

  virtio_device_reset(dev);
  set_status(dev, VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER);

  virtio_mmio_read_features(mmio_base, &dev->device_features);
  #if VIRTIO_DEBUG
  printf("virtio: features=0x%{type: hex}\n", PRINT_FLAG_BOTH, dev->device_features);
  #endif
  return 0;
}

int virtio_device_negotiate(virtio_device_t *dev, uint64_t wanted, uint64_t required, uint64_t *out) {
  if (!dev)
    return -1;
  uint64_t offered = dev->device_features;
  uint64_t chosen = offered & wanted;
  if ((required & ~offered) != 0)
    return -2; // required bit missing

  chosen |= (required & offered);

  // Always set VERSION_1
  if (!(offered & VIRTIO_F_VERSION_1))
    return -3;
  chosen |= VIRTIO_F_VERSION_1;

  virtio_mmio_write_driver_features(dev->mmio_base, chosen);
  set_status(dev, VIRTIO_CONFIG_S_FEATURES_OK);

  uint32_t st = virtio_mmio_read32(dev->mmio_base, VIRTIO_MMIO_STATUS);
  if ((st & VIRTIO_CONFIG_S_FEATURES_OK) == 0)
    return -4; // device cleared FEATURES_OK

  dev->negotiated_features = chosen;
  #if VIRTIO_DEBUG
  printf("virtio: negotiated=0x%{type: hex}\n", PRINT_FLAG_BOTH, chosen);
  #endif
  dev->is_initialized = true;
  if (out) *out = chosen;
  return 0;
}



#pragma once

#include <stdint.h>

static inline uint32_t virtio_mmio_read32(uintptr_t base, uint32_t off) {
  return *(volatile uint32_t *)(base + off);
}

static inline void virtio_mmio_write32(uintptr_t base, uint32_t off, uint32_t v) {
  *(volatile uint32_t *)(base + off) = v;
}

static inline void virtio_mmio_read_features(uintptr_t base, uint64_t *out_features) {
  // Read 64-bit features via two 32-bit windows (select 0/1)
  *(volatile uint32_t *)(base + 0x014u) = 0; // DEVICE_FEATURES_SEL = 0
  uint32_t lo = *(volatile uint32_t *)(base + 0x010u);
  *(volatile uint32_t *)(base + 0x014u) = 1; // DEVICE_FEATURES_SEL = 1
  uint32_t hi = *(volatile uint32_t *)(base + 0x010u);
  *out_features = ((uint64_t)hi << 32) | lo;
}

static inline void virtio_mmio_write_driver_features(uintptr_t base, uint64_t features) {
  *(volatile uint32_t *)(base + 0x024u) = 0; // DRIVER_FEATURES_SEL = 0
  *(volatile uint32_t *)(base + 0x020u) = (uint32_t)(features & 0xFFFFFFFFu);
  *(volatile uint32_t *)(base + 0x024u) = 1; // DRIVER_FEATURES_SEL = 1
  *(volatile uint32_t *)(base + 0x020u) = (uint32_t)(features >> 32);
}



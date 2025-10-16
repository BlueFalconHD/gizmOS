#pragma once

#include <stdint.h>

// VirtIO 1.1 (modern) over MMIO – register layout and shared structures
// Spec: https://docs.oasis-open.org/virtio/virtio/v1.1/

// MMIO register offsets
#define VIRTIO_MMIO_MAGIC_VALUE        0x000u
#define VIRTIO_MMIO_VERSION            0x004u
#define VIRTIO_MMIO_DEVICE_ID          0x008u
#define VIRTIO_MMIO_VENDOR_ID          0x00Cu

#define VIRTIO_MMIO_DEVICE_FEATURES    0x010u
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014u

#define VIRTIO_MMIO_DRIVER_FEATURES    0x020u
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024u

#define VIRTIO_MMIO_QUEUE_SEL          0x030u
#define VIRTIO_MMIO_QUEUE_NUM_MAX      0x034u
#define VIRTIO_MMIO_QUEUE_NUM          0x038u

#define VIRTIO_MMIO_QUEUE_READY        0x044u
#define VIRTIO_MMIO_QUEUE_NOTIFY       0x050u

#define VIRTIO_MMIO_INTERRUPT_STATUS   0x060u
#define VIRTIO_MMIO_INTERRUPT_ACK      0x064u

#define VIRTIO_MMIO_STATUS             0x070u

#define VIRTIO_MMIO_QUEUE_DESC_LOW     0x080u
#define VIRTIO_MMIO_QUEUE_DESC_HIGH    0x084u
#define VIRTIO_MMIO_DRIVER_DESC_LOW    0x090u  // avail ring
#define VIRTIO_MMIO_DRIVER_DESC_HIGH   0x094u
#define VIRTIO_MMIO_DEVICE_DESC_LOW    0x0A0u  // used ring
#define VIRTIO_MMIO_DEVICE_DESC_HIGH   0x0A4u

#define VIRTIO_MMIO_CONFIG             0x100u  // device-specific config base

// Device IDs (subset we support)
#define VIRTIO_DEVICE_BLOCK            2u
#define VIRTIO_DEVICE_CONSOLE          3u
#define VIRTIO_DEVICE_INPUT            18u

// Status bits (virtio_config.h)
#define VIRTIO_CONFIG_S_ACKNOWLEDGE    (1u << 0)
#define VIRTIO_CONFIG_S_DRIVER         (1u << 1)
#define VIRTIO_CONFIG_S_DRIVER_OK      (1u << 2)
#define VIRTIO_CONFIG_S_FEATURES_OK    (1u << 3)
#define VIRTIO_CONFIG_S_NEEDS_RESET    (1u << 6)

// Common feature bits
#define VIRTIO_F_VERSION_1             (1ULL << 32)
#define VIRTIO_RING_F_INDIRECT_DESC    (1ULL << 28)
#define VIRTIO_RING_F_EVENT_IDX        (1ULL << 29)

// Virtqueue structures (spec §2.6)
struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

#define VRING_DESC_F_NEXT              1u
#define VRING_DESC_F_WRITE             2u

struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[]; // size decided at runtime
} __attribute__((packed));

struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[]; // size decided at runtime
} __attribute__((packed));

// Virtio Input (spec §5.8) event structure
struct virtio_input_event {
    uint16_t type;
    uint16_t code;
    uint32_t value;
} __attribute__((packed));



//
// Minimal VirtIO MMIO / virtqueue definitions
// – tailored for gizmOS input‑device drivers
//
// Spec: https://docs.oasis-open.org/virtio/virtio/v1.1/
//
// NOTE: Only the offsets & flags actually used in‑tree are kept.
//       Add more as additional VirtIO devices are implemented.
//
#pragma once
#include <stdint.h>

/* --------------------------------------------------------------------------
 *  MMIO register offsets (base is 0x10001000 in QEMU virtio‑mmio)
 * -------------------------------------------------------------------------- */
#define VIRTIO_MMIO_MAGIC_VALUE      0x000   /* 0x74726976 */
#define VIRTIO_MMIO_VERSION          0x004   /* should read 2               */
#define VIRTIO_MMIO_DEVICE_ID        0x008   /* 18 == input, 1 == net …     */
#define VIRTIO_MMIO_VENDOR_ID        0x00c   /* 0x554d4551 (“QEMU”)         */
#define VIRTIO_MMIO_DEVICE_FEATURES  0x010
#define VIRTIO_MMIO_DRIVER_FEATURES  0x020
#define VIRTIO_MMIO_QUEUE_SEL        0x030   /* write queue number          */
#define VIRTIO_MMIO_QUEUE_NUM_MAX    0x034   /* max descriptors (ro)        */
#define VIRTIO_MMIO_QUEUE_NUM        0x038   /* write queue size            */
#define VIRTIO_MMIO_QUEUE_READY      0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY     0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060
#define VIRTIO_MMIO_INTERRUPT_ACK    0x064
#define VIRTIO_MMIO_STATUS           0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW   0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH  0x084
#define VIRTIO_MMIO_DRIVER_DESC_LOW  0x090
#define VIRTIO_MMIO_DRIVER_DESC_HIGH 0x094
#define VIRTIO_MMIO_DEVICE_DESC_LOW  0x0a0
#define VIRTIO_MMIO_DEVICE_DESC_HIGH 0x0a4

/* --------------------------------------------------------------------------
 *  Status‑register bits (virtio_config.h)
 * -------------------------------------------------------------------------- */
#define VIRTIO_CONFIG_S_ACKNOWLEDGE  (1 << 0)
#define VIRTIO_CONFIG_S_DRIVER       (1 << 1)
#define VIRTIO_CONFIG_S_DRIVER_OK    (1 << 2)
#define VIRTIO_CONFIG_S_FEATURES_OK  (1 << 3)

/* --------------------------------------------------------------------------
 *  Virtqueue structures (spec §2.6)
 * -------------------------------------------------------------------------- */
struct virtq_desc {
    uint64_t addr;   /* guest‑physical buffer address           */
    uint32_t len;    /* buffer length in bytes                  */
    uint16_t flags;  /* VRING_DESC_F_*                          */
    uint16_t next;   /* next desc in chain (if NEXT set)        */
} __attribute__((packed));

#define VRING_DESC_F_NEXT  1u   /* chained with another descriptor */
#define VRING_DESC_F_WRITE 2u   /* device writes (vs. read)        */

/* ---------- available ring (driver → device) ---------- */
struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];          /* descriptor heads – size set at runtime */
} __attribute__((packed));

/* ---------- used ring (device → driver) --------------- */
struct virtq_used_elem {
    uint32_t id;   /* index of start of completed chain */
    uint32_t len;  /* total bytes written               */
} __attribute__((packed));

struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[];   /* size set at runtime */
} __attribute__((packed));

/* --------------------------------------------------------------------------
 *  Input‑device event (spec §5.8.6)
 * -------------------------------------------------------------------------- */
struct virtio_input_event {
    uint16_t type;   /* EV_* family (e.g. EV_KEY) */
    uint16_t code;   /* KEY_* or REL_* etc.       */
    uint32_t value;  /* 1=press, 0=release, 2=repeat */
} __attribute__((packed));

/* --------------------------------------------------------------------------
 *  Default MMIO base / IRQ for QEMU’s first virtio‑mmio device
 *  (override in platform code if different)
 * -------------------------------------------------------------------------- */
#define VIRTIO0      0x10001000UL
#define VIRTIO0_IRQ  1

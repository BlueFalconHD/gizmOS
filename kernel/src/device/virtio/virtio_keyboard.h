#pragma once

#include "virtio_common.h"
#include "virtio.h"

#define VIRTIO_KEYBOARD_EVENT_NUM 16

/** single‑byte DRIVER_OK packet for ctrl queue */
struct virtio_keyboard_status_pkt {
  uint16_t select;   // VIRTIO_INPUT_CFG_ID_STATUS == 1
  uint16_t reserved; // 0
  uint32_t size;     // 1
  uint8_t  data;     // 1 == DRIVER_OK
} __attribute__((packed));

typedef struct {
  /* base VirtIO wrapper */
  virtio_device_t vdev;

  /* Event queue 0 */
  virtio_queue_t  q_events;
  struct virtio_input_event events[VIRTIO_KEYBOARD_EVENT_NUM];

  /* ➜ NEW: Control queue 1 */
  virtio_queue_t  q_ctl;
  struct virtio_keyboard_status_pkt status_pkt;
} virtio_keyboard_t;

RESULT_TYPE(virtio_keyboard_t *) make_virtio_keyboard(uint64_t base, uint32_t irq);

g_bool virtio_keyboard_init(virtio_keyboard_t *kbd);

void virtio_keyboard_handle_irq(virtio_keyboard_t *kbd);

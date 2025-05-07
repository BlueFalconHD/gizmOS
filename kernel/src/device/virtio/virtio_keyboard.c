#include "virtio_keyboard.h"
#include "device/virtio/virtio_keycode.h"
#include <lib/memory.h>
#include <lib/panic.h>
#include <lib/print.h>

RESULT_TYPE(virtio_keyboard_t *)
make_virtio_keyboard(uint64_t base, uint32_t irq) {
  virtio_keyboard_t *kbd = (virtio_keyboard_t *)alloc_page();
  if (!kbd)
    return RESULT_FAILURE(RESULT_NOMEM);

  kbd->vdev.base = base;
  kbd->vdev.irq = irq;
  kbd->vdev.is_initialized = false;

  /* ➜ NEW: populate static status packet */
  kbd->status_pkt = (struct virtio_keyboard_status_pkt){
      .select = 1, .reserved = 0, .size = 1, .data = 1 /* DRIVER_OK */
  };

  return RESULT_SUCCESS(kbd);
}

g_bool virtio_keyboard_init(virtio_keyboard_t *kbd) {
  if (!kbd)
    return false;

  if (!virtio_device_init(&kbd->vdev, 0))
    return false;

  /* Queue 0 – event stream */
  if (!virtio_queue_setup(&kbd->vdev, &kbd->q_events,
                          /*qsel*/ 0, VIRTIO_KEYBOARD_EVENT_NUM, kbd->events,
                          sizeof(struct virtio_input_event)))
    return false;

  if (!virtio_queue_setup(&kbd->vdev, &kbd->q_ctl,
                          /*qsel*/ 1, /*size*/ 1, &kbd->status_pkt,
                          sizeof(struct virtio_keyboard_status_pkt)))
    return false;

  /* override descriptor flags – device reads this */
  kbd->q_ctl.desc[0].flags = 0; /* clear VRING_DESC_F_WRITE */

  /* push packet */
  kbd->q_ctl.avail->ring[0] = 0;
  kbd->q_ctl.avail->idx = 1;
  __sync_synchronize();
  virtio_mmio_write(&kbd->vdev, VIRTIO_MMIO_QUEUE_NOTIFY, 1);

  /* finally signal DRIVER_OK */
  virtio_mmio_write(&kbd->vdev, VIRTIO_MMIO_STATUS,
                    virtio_mmio_read(&kbd->vdev, VIRTIO_MMIO_STATUS) |
                        VIRTIO_CONFIG_S_DRIVER_OK);

  return true;
}

#define EV_SYN 0x00
#define EV_KEY 0x01

static const char *ev_type_str(uint16_t t) {
  switch (t) {
  case EV_SYN:
    return "EV_SYN";
  case EV_KEY:
    return "EV_KEY";
  default:
    return "UNKNOWN";
  }
}

// value: indicates key state (pressed/released)
#define KEY_PRESSED 1
#define KEY_RELEASED 0

static const char *key_state_str(uint16_t state) {
  switch (state) {
  case KEY_PRESSED:
    return "KEY_PRESSED";
  case KEY_RELEASED:
    return "KEY_RELEASED";
  default:
    return "UNKNOWN";
  }
}

void virtio_keyboard_handle_irq(virtio_keyboard_t *kbd) {
  if (!kbd || !kbd->vdev.is_initialized)
    return;

  virtio_ack_irq(&kbd->vdev);

  while (kbd->q_events.last_used_idx != kbd->q_events.used->idx) {
    uint16_t pos = kbd->q_events.last_used_idx % kbd->q_events.size;
    uint16_t id = kbd->q_events.used->ring[pos].id;
    __sync_synchronize();
    struct virtio_input_event *ev = &kbd->events[id];

    // printf("[kbd] type=%{type: str} code=%{type: hex} value=%{type: hex}\n",
    //        PRINT_FLAG_BOTH, ev_type_str(ev->type), ev->code, ev->value);

    // check if it's a key event
    if (ev->type == EV_KEY &&
        (ev->value == KEY_PRESSED || ev->value == KEY_RELEASED)) {
      const char *kc = virtio_keycode_to_string(ev->code);

      if (ev->value == KEY_PRESSED) {
        printf("v %{type: str}, ", PRINT_FLAG_BOTH, kc);
      } else {
        printf("^ %{type: str}, ", PRINT_FLAG_BOTH, kc);
      }
    }

    kbd->q_events.avail->ring[kbd->q_events.avail->idx % kbd->q_events.size] =
        id;
    kbd->q_events.avail->idx++;
    kbd->q_events.last_used_idx++;
  }

  __sync_synchronize();
  virtio_mmio_write(&kbd->vdev, VIRTIO_MMIO_QUEUE_NOTIFY, 0);
}

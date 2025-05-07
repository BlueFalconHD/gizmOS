#include "virtio_mouse.h"
#include "device/shared.h"
#include "lib/sbi.h"
#include <lib/memory.h>
#include <lib/panic.h>
#include <lib/print.h>

/* -------------------------------------------------------------------------- */
/*  Constructor / initialisation                                              */
/* -------------------------------------------------------------------------- */
RESULT_TYPE(virtio_mouse_t *)
make_virtio_mouse(uint64_t base, uint32_t irq) {
  virtio_mouse_t *m = (virtio_mouse_t *)alloc_page();
  if (!m)
    return RESULT_FAILURE(RESULT_NOMEM);

  m->vdev.base = base;
  m->vdev.irq = irq;
  m->vdev.is_initialized = false;

  m->status_pkts[0] = (struct virtio_mouse_status_pkt){
      .select = 1, .reserved = 0, .size = 1, .data = 1};

  m->rel_x = m->rel_y = m->wheel = 0;
  m->buttons = 0;
  return RESULT_SUCCESS(m);
}

g_bool virtio_mouse_init(virtio_mouse_t *m) {
  if (!m)
    return false;

  /* device‑level init + feature negotiation (we need none) */
  if (!virtio_device_init(&m->vdev, 0))
    return false;

  /* queue 0 – event ring */
  if (!virtio_queue_setup(&m->vdev, &m->q_events, /*qsel*/ 0,
                          VIRTIO_MOUSE_EVENT_NUM, m->events,
                          sizeof(struct virtio_input_event)))
    return false;

  /* queue 1 – control / status (tiny single descriptor) */
  if (!virtio_queue_setup(&m->vdev, &m->q_ctl, /*qsel*/ 1,
                          /*size*/ 8, &m->status_pkts,
                          sizeof(struct virtio_mouse_status_pkt)))
    return false;

  /* descriptor is read‑only for the device */
  m->q_ctl.desc[0].flags = 0; /* clear VRING_DESC_F_WRITE */
  m->q_ctl.avail->ring[0] = 0;
  m->q_ctl.avail->idx = 1;
  __sync_synchronize();
  virtio_mmio_write(&m->vdev, VIRTIO_MMIO_QUEUE_NOTIFY, 1);

  /* finally → DRIVER_OK */
  virtio_mmio_write(&m->vdev, VIRTIO_MMIO_STATUS,
                    virtio_mmio_read(&m->vdev, VIRTIO_MMIO_STATUS) |
                        VIRTIO_CONFIG_S_DRIVER_OK);
  return true;
}

/* -------------------------------------------------------------------------- */
/*  IRQ handler – drain queue 0 and update simple state                       */
/* -------------------------------------------------------------------------- */
#define EV_SYN 0x00
#define EV_KEY 0x01
#define EV_REL 0x02

static const char *rel_code_str(uint16_t c) {
  switch (c) {
  case REL_X:
    return "REL_X";
  case REL_Y:
    return "REL_Y";
  case REL_WHEEL:
    return "REL_WHEEL";
  default:
    return "REL_UNKNOWN";
  }
}
static const char *btn_code_str(uint16_t c) {
  switch (c) {
  case BTN_LEFT:
    return "BTN_LEFT";
  case BTN_RIGHT:
    return "BTN_RIGHT";
  case BTN_MIDDLE:
    return "BTN_MIDDLE";
  default:
    return "BTN?";
  }
}

void virtio_mouse_handle_irq(virtio_mouse_t *m) {
  if (!m || !m->vdev.is_initialized)
    return;

  virtio_ack_irq(&m->vdev);

  /* drain used ring */
  while (m->q_events.last_used_idx != m->q_events.used->idx) {
    uint16_t pos = m->q_events.last_used_idx % m->q_events.size;
    uint16_t id = m->q_events.used->ring[pos].id;
    struct virtio_input_event *ev = &m->events[id];

    switch (ev->type) {
    case EV_REL:
      if (ev->code == REL_X)
        m->rel_x += (int32_t)ev->value;
      else if (ev->code == REL_Y)
        m->rel_y += (int32_t)ev->value;
      else if (ev->code == REL_WHEEL)
        m->wheel += (int32_t)ev->value;

      // printf("[mouse] %{type: str} %{type: int}\n", PRINT_FLAG_BOTH,
      //        rel_code_str(ev->code), (int)ev->value);

      if (shared_cursor_initialized && (m->rel_x != 0 || m->rel_y != 0)) {
        cursor_move(shared_cursor, m->rel_x, m->rel_y);
        m->rel_x = m->rel_y = 0; /* consumed */
      }

      break;

    case EV_KEY: { /* buttons */
      uint8_t mask = 0;
      if (ev->code == BTN_LEFT)
        mask = 1 << 0;
      if (ev->code == BTN_RIGHT)
        mask = 1 << 1;
      if (ev->code == BTN_MIDDLE)
        mask = 1 << 2;
      if (ev->value)
        m->buttons |= mask; /* press   */
      else
        m->buttons &= ~mask; /* release */

      if (ev->code == BTN_LEFT && ev->value) {
        sbi_system_reset(SBI_SRST_TYPE_COLD_REBOOT, SBI_SRST_REASON_NONE);
      }

      break;
    }

    case EV_SYN:
    default:
      break;
    }

    /* recycle descriptor */
    m->q_events.avail->ring[m->q_events.avail->idx % m->q_events.size] = id;
    m->q_events.avail->idx++;
    m->q_events.last_used_idx++;
  }

  __sync_synchronize();
  virtio_mmio_write(&m->vdev, VIRTIO_MMIO_QUEUE_NOTIFY, 0);
}

#include "input.h"
#include "../core/mmio.h"
#include "../core/queue.h"
#include <device/cursor.h>
#include <device/shared.h>
#include <lib/kalloc.h>
#include <lib/keyboard.h>
#include <lib/memory.h>
#include <lib/print.h>
#include <page_table.h>
#include <proc/notification.h>
#include <proc/notification_types.h>
#include <proc/process.h>
#include <proc/process_table.h>
#include <stddef.h>

#ifndef INPUT_DEBUG_LEVEL
#define INPUT_DEBUG_LEVEL 0
#endif

typedef struct {
  uint16_t select;
  uint16_t reserved;
  uint32_t size;
  uint8_t data;
} __attribute__((packed)) virtio_input_status_pkt_t;

// Maintain a small registry of input instances keyed by device pointer
#define MAX_INPUT_DEVS 4
typedef struct {
  virtio_device_t *dev;
  virtio_input_dev_t *inp;
} input_slot_t;
static input_slot_t g_inputs[MAX_INPUT_DEVS];

// Allow one primary callback (kbd or mouse) and an additional listener.
// This keeps compatibility while ensuring both kbd and mouse can receive
// events. Deprecated global extras; store in the instance now

static void event_used_cb(struct virtq *q, void *cookie, size_t bytes,
                          void *user) {
  (void)bytes;
  (void)q;
  virtio_input_dev_t *inp = (virtio_input_dev_t *)user;
  uint16_t idx = (uint16_t)(uintptr_t)cookie;
  if (inp->on_event) {
    inp->on_event(&inp->events[idx], inp->user);
  }
  if (inp->extra_cb) {
    inp->extra_cb(&inp->events[idx], inp->extra_user);
  }
  // Requeue the same slot to keep the ring full
  struct iovec in_sg = {.iov_base = &inp->events[idx],
                        .iov_len = sizeof(struct virtio_input_event)};
  virtq_submit(inp->q_events, NULL, 0, &in_sg, 1, (void *)(uintptr_t)idx);
  virtq_kick(inp->vdev, 0);
}

static int input_init(virtio_input_dev_t *inp, uint16_t ring_size) {
  uint64_t out;
  int rc = virtio_device_negotiate(inp->vdev, 0, VIRTIO_F_VERSION_1, &out);
  if (rc)
    return rc;

  // Create event queue 0
  rc = virtq_create(inp->vdev, 0, ring_size, &inp->q_events);
  if (rc)
    return rc;

  // Create control queue 1 and post a DRIVER_OK status packet
  rc = virtq_create(inp->vdev, 1, 1, &inp->q_ctl);
  if (rc)
    return rc;
  virtio_input_status_pkt_t *pkt =
      (virtio_input_status_pkt_t *)kalloc(sizeof(virtio_input_status_pkt_t));
  pkt->select = 1; // STATUS
  pkt->reserved = 0;
  pkt->size = 1;
  pkt->data = 1; // DRIVER_OK

  struct iovec out_sg[1] = {{.iov_base = pkt, .iov_len = sizeof(*pkt)}};
  virtq_submit(inp->q_ctl, out_sg, 1, NULL, 0, (void *)(uintptr_t)0);
  virtq_kick(inp->vdev, 1);

  // Post event buffers
  inp->events = (struct virtio_input_event *)kalloc(
      sizeof(struct virtio_input_event) * ring_size);
  inp->event_cap = ring_size;
  for (uint16_t i = 0; i < ring_size; i++) {
    struct iovec in_sg = {.iov_base = &inp->events[i],
                          .iov_len = sizeof(struct virtio_input_event)};
    virtq_submit(inp->q_events, NULL, 0, &in_sg, 1, (void *)(uintptr_t)i);
  }
  virtq_kick(inp->vdev, 0);

  // Install callback to deliver events up and requeue buffers
  virtq_set_callback(inp->q_events, event_used_cb, inp);

  // Driver OK
  uint32_t s = virtio_mmio_read32(inp->vdev->mmio_base, VIRTIO_MMIO_STATUS);
  virtio_mmio_write32(inp->vdev->mmio_base, VIRTIO_MMIO_STATUS,
                      s | VIRTIO_CONFIG_S_DRIVER_OK);
  return 0;
}

int virtio_input_probe(virtio_device_t *dev, uint16_t event_ring_size) {
  // check existing
  for (int i = 0; i < MAX_INPUT_DEVS; i++) {
    if (g_inputs[i].dev == dev)
      return 0;
  }
  // find free slot
  int free_idx = -1;
  for (int i = 0; i < MAX_INPUT_DEVS; i++) {
    if (g_inputs[i].dev == NULL) {
      free_idx = i;
      break;
    }
  }
  if (free_idx < 0)
    return -1;
  virtio_input_dev_t *inp =
      (virtio_input_dev_t *)kalloc(sizeof(virtio_input_dev_t));
  if (!inp)
    return -1;
  memset(inp, 0, sizeof(virtio_input_dev_t));
  inp->vdev = dev;
  int rc = input_init(inp, event_ring_size);
  if (rc)
    return rc;
  g_inputs[free_idx].dev = dev;
  g_inputs[free_idx].inp = inp;
  return 0;
}

void virtio_input_set_callback(virtio_input_dev_t *inp,
                               virtio_input_event_cb cb, void *user) {
  inp->on_event = cb;
  inp->user = user;
}

virtio_input_dev_t *virtio_input_get(void) {
  // return first for backward compatibility
  for (int i = 0; i < MAX_INPUT_DEVS; i++)
    if (g_inputs[i].dev)
      return g_inputs[i].inp;
  return NULL;
}
virtio_input_dev_t *virtio_input_get_for(virtio_device_t *dev) {
  for (int i = 0; i < MAX_INPUT_DEVS; i++)
    if (g_inputs[i].dev == dev)
      return g_inputs[i].inp;
  return NULL;
}

int virtio_input_add_listener(virtio_input_dev_t *inp, virtio_input_event_cb cb,
                              void *user) {
  inp->extra_cb = cb;
  inp->extra_user = user;
  return 0;
}

static void unified_on_event(const struct virtio_input_event *ev, void *user) {
  virtio_input_dev_t *inp = (virtio_input_dev_t *)user;

  // EV_REL: mouse motion and wheel
  if (ev->type == 0x02) {
    if (ev->code == 0x00)
      inp->mouse_rel_x += (int32_t)ev->value; // REL_X
    if (ev->code == 0x01)
      inp->mouse_rel_y += (int32_t)ev->value; // REL_Y
    if (ev->code == 0x08)
      inp->mouse_wheel += (int32_t)ev->value; // REL_WHEEL

    if (shared_cursor_initialized && (inp->mouse_rel_x || inp->mouse_rel_y)) {
      cursor_move(shared_cursor, inp->mouse_rel_x, inp->mouse_rel_y);
      inp->mouse_rel_x = 0;
      inp->mouse_rel_y = 0;
    }
    return;
  }

  // EV_KEY: keyboard keys and mouse buttons
  if (ev->type == 0x01) {
    // BTN_LEFT=0x110, BTN_RIGHT=0x111, BTN_MIDDLE=0x112
    if (ev->code == 0x110 || ev->code == 0x111 || ev->code == 0x112) {
      uint8_t mask = 0;
      if (ev->code == 0x110)
        mask = 1u << 0;
      if (ev->code == 0x111)
        mask = 1u << 1;
      if (ev->code == 0x112)
        mask = 1u << 2;
      if (ev->value)
        inp->mouse_buttons |= mask;
      else
        inp->mouse_buttons &= (uint8_t)~mask;
      return;
    }

    // Track modifier keys
    switch (ev->code) {
    case 0x3a: // KEY_CAPSLOCK
      KEYPRESS_MODIFIER_SET(inp->kbd_modifiers, KEYPRESS_MODIFIER_CAPSLOCK,
                            ev->value == 1);
      return;
    case 42: // KEY_LEFTSHIFT
      KEYPRESS_MODIFIER_SET(inp->kbd_modifiers, KEYPRESS_MODIFIER_LSHIFT,
                            ev->value != 0);
      break;
    case 54: // KEY_RIGHTSHIFT
      KEYPRESS_MODIFIER_SET(inp->kbd_modifiers, KEYPRESS_MODIFIER_RSHIFT,
                            ev->value != 0);
      break;
    case 29: // KEY_LEFTCTRL
      KEYPRESS_MODIFIER_SET(inp->kbd_modifiers, KEYPRESS_MODIFIER_LCTRL,
                            ev->value != 0);
      break;
    case 97: // KEY_RIGHTCTRL
      KEYPRESS_MODIFIER_SET(inp->kbd_modifiers, KEYPRESS_MODIFIER_RCTRL,
                            ev->value != 0);
      break;
    case 56: // KEY_LEFTALT
      KEYPRESS_MODIFIER_SET(inp->kbd_modifiers, KEYPRESS_MODIFIER_LALT,
                            ev->value != 0);
      break;
    case 100: // KEY_RIGHTALT
      KEYPRESS_MODIFIER_SET(inp->kbd_modifiers, KEYPRESS_MODIFIER_RALT,
                            ev->value != 0);
      break;
    case 125: // KEY_LEFTMETA
      KEYPRESS_MODIFIER_SET(inp->kbd_modifiers, KEYPRESS_MODIFIER_LMETA,
                            ev->value != 0);
      break;
    case 126: // KEY_RIGHTMETA
      KEYPRESS_MODIFIER_SET(inp->kbd_modifiers, KEYPRESS_MODIFIER_RMETA,
                            ev->value != 0);
      break;
    default:
      break;
    }

    // Broadcast keypress via notification system to any user process
    // that registered a handler for NOTIF_TYPE_KEYPRESS.
    keypress_t kp = {
        .keycode = (uint8_t)ev->code,
        .modifiers = inp->kbd_modifiers,
        .type = (ev->value != 0) ? KEYBOARD_KEY_PRESSED : KEYBOARD_KEY_RELEASED,
    };

#if INPUT_DEBUG_LEVEL >= 2
    printf("[input] key event code=%{type: int} val=%{type: int} mods=%{type: "
           "int}\n",
           PRINT_FLAG_BOTH, (int)ev->code, (int)ev->value,
           (int)inp->kbd_modifiers);
#endif

    for (uint8_t i = 0; i < NPROC; i++) {
      proc_t *p = &processes[i];
      acquire(&p->lock);
      g_bool deliver = (p->state != UNUSED) && (p->is_kernel == 0) &&
                       (p->notif_handlers[NOTIF_TYPE_KEYPRESS].handler_va != 0);
      release(&p->lock);
      if (deliver) {
        g_bool ok =
            notification_post_copy(p, NOTIF_TYPE_KEYPRESS, &kp, sizeof(kp), 0);
#if INPUT_DEBUG_LEVEL < 1
        (void)ok;
#endif
#if INPUT_DEBUG_LEVEL >= 1
        printf("[input] post keypress to pid=%{type: int} ok=%{type: int}\n",
               PRINT_FLAG_BOTH, p->pid, ok ? 1 : 0);
#endif
      }
    }
    return;
  }
}

int virtio_input_driver_probe(virtio_device_t *dev) {
  int rc = virtio_input_probe(dev, 16);
  if (rc)
    return rc;
  virtio_input_dev_t *inp = virtio_input_get_for(dev);
  if (!inp)
    return -1;
  inp->kbd_modifiers = 0;
  inp->mouse_rel_x = inp->mouse_rel_y = 0;
  inp->mouse_wheel = 0;
  inp->mouse_buttons = 0;
  virtio_input_set_callback(inp, unified_on_event, inp);
  return 0;
}

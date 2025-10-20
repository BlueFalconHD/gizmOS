#include "irq.h"
#include "device.h"
#include "mmio.h"
#include "queue.h"
#include "spec.h"
#include <lib/log.h>
#include <lib/memory.h>
#include <lib/print.h>

static inline log_t *virtio_irq_log() {
  static log_t *l = NULL;
  if (!l) {
    l = g_log_create("virtio", "irq");
#if VIRTIO_DEBUG
    g_log_set_level(l, LOG_LEVEL_DEBUG);
#else
    g_log_set_level(l, LOG_LEVEL_INFO);
#endif
  }
  return l;
}

#include <device/virtio/virtio.h>
#include <lib/print.h>

// Simple per-device callback store (small system, static for now)
typedef struct {
  virtio_device_t *dev;
  virtio_config_changed_cb cb;
} virtio_cb_slot_t;

#define VIRTIO_MAX_DEVS 8
static virtio_cb_slot_t g_cb[VIRTIO_MAX_DEVS];

void virtio_set_config_changed_callback(virtio_device_t *dev,
                                        virtio_config_changed_cb cb) {
  for (int i = 0; i < VIRTIO_MAX_DEVS; i++) {
    if (g_cb[i].dev == NULL || g_cb[i].dev == dev) {
      g_cb[i].dev = dev;
      g_cb[i].cb = cb;
      return;
    }
  }
}

void virtio_shared_isr(uint32_t irq) {
  (void)irq; // The platform code should map IRQ -> device if needed.
  for (int i = 0; i < VIRTIO_MAX_DEVS; i++) {
    virtio_device_t *dev = g_cb[i].dev;
    if (!dev)
      continue;
    uint32_t ist =
        virtio_mmio_read32(dev->mmio_base, VIRTIO_MMIO_INTERRUPT_STATUS);
    if (ist == 0)
      continue;
    virtio_mmio_write32(dev->mmio_base, VIRTIO_MMIO_INTERRUPT_ACK, ist);
    LOG_DEBUG(virtio_irq_log(), "IRQ%{type: int} ist=0x%{type: hex}", irq,
              (uint64_t)ist);
    if ((ist & 0x2) && g_cb[i].cb) { // bit1: config change
      g_cb[i].cb(dev);
    }
    if (ist & 0x1) { // bit0: used ring update
      for (uint16_t q = 0; q < dev->num_queues; q++) {
        if (dev->queues[q]) {
          // Only drain queues that have a completion callback installed.
          // Queues without callbacks (e.g., synchronous block I/O) manage
          // completions via their own polling.
          if (dev->queues[q]->on_used) {
            while (virtq_poll_used(dev->queues[q], NULL, NULL) == 0) {
              // keep draining; callbacks invoked inside poll
            }
          }
        }
      }
    }
  }
}

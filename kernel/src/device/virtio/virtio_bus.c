#include "virtio_bus.h"
#include <lib/memory.h>
#include <lib/panic.h>
#include <lib/print.h>

#define VIRTIO_MAX_DEVICES 8

static virtio_device_t *g_devices[VIRTIO_MAX_DEVICES];
static uint32_t         g_count = 0;

void virtio_bus_init(void) {
  for (uint32_t i = 0; i < VIRTIO_MAX_DEVICES; i++)
    g_devices[i] = 0;
  g_count = 0;
}

g_bool virtio_bus_register(virtio_device_t *dev) {
  if (!dev)
    return false;
  if (g_count >= VIRTIO_MAX_DEVICES)
    return false;
  g_devices[g_count++] = dev;
  return true;
}

virtio_device_t *virtio_bus_get_by_irq(uint32_t irq) {
  for (uint32_t i = 0; i < g_count; i++) {
    if (g_devices[i] && g_devices[i]->irq == irq)
      return g_devices[i];
  }
  return 0;
}

void virtio_bus_handle_irq(uint32_t irq) {
  virtio_device_t *dev = virtio_bus_get_by_irq(irq);
  if (!dev)
    return;
  /* ack first to de-assert line */
  virtio_ack_irq(dev);
  if (dev->isr) {
    dev->isr(dev->driver_data);
  }
}



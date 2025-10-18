#include "registry.h"
#include "irq.h"
#include <device/virtio/virtio.h>
#include <dtb/dtb.h>
#include <extern/smoldtb/smoldtb.h>
#include <lib/kalloc.h>
#include <lib/print.h>
#include <lib/log.h>

static inline log_t *virtio_reg_log() {
  static log_t *l = NULL;
  if (!l) {
    l = g_log_create("virtio", "registry");
    #if VIRTIO_DEBUG
    g_log_set_level(l, LOG_LEVEL_DEBUG);
    #else
    g_log_set_level(l, LOG_LEVEL_INFO);
    #endif
  }
  return l;
}


#define MAX_DRIVERS 8
#define MAX_DEVS 8

static const virtio_driver_t *g_drivers[MAX_DRIVERS];
static uint32_t g_driver_count = 0;

int virtio_register_driver(const virtio_driver_t *drv) {
  if (!drv || g_driver_count >= MAX_DRIVERS)
    return -1;
  g_drivers[g_driver_count++] = drv;
  return 0;
}

// legacy single-match finder kept for reference
__attribute__((unused)) static const virtio_driver_t *
find_driver(uint32_t device_id) {
  for (uint32_t i = 0; i < g_driver_count; i++) {
    if (g_drivers[i]->device_id == device_id)
      return g_drivers[i];
  }
  return 0;
}

void virtio_bus_init_from_dtb(void) {
  // Find all nodes compatible with "virtio,mmio"
  dtb_node *node = dtb_find("/");
  if (!node)
    return;

  // Walk tree to find compat; smoldtb offers dtb_find_compatible for subtree.
  dtb_node *cur = node;
  while ((cur = dtb_find_compatible(cur, "virtio,mmio")) != NULL) {
    size_t addr_cells = dtb_get_addr_cells_for(cur);
    size_t size_cells = dtb_get_size_cells_for(cur);
    dtb_prop *reg_prop = dtb_find_prop(cur, "reg");
    if (!reg_prop)
      break;
    dtb_pair pair;
    // read first pair only (base,size)
    dtb_read_prop_2(reg_prop, (dtb_pair){addr_cells, size_cells}, &pair);
    uintptr_t base = (uintptr_t)pair.a;
#if VIRTIO_DEBUG
    LOG_DEBUG(virtio_reg_log(), "mmio base=0x%{type: hex}", (uint64_t)base);
#endif

    // Interrupts: assume integer value in "interrupts"; platform wires it
    uint32_t irq = 0;
    dtb_prop *irq_prop = dtb_find_prop(cur, "interrupts");
    if (irq_prop) {
      uintmax_t val = 0;
      dtb_read_prop_1(irq_prop, 1, &val);
      irq = (uint32_t)val;
    }
#if VIRTIO_DEBUG
    LOG_DEBUG(virtio_reg_log(), "irq=%{type: int}", irq);
#endif

    // Probe device header
    virtio_device_t *dev = (virtio_device_t *)kalloc(sizeof(virtio_device_t));
    if (!dev)
      return;
    if (virtio_device_init(dev, base, irq) != 0)
      continue;

#if VIRTIO_DEBUG
    LOG_DEBUG(virtio_reg_log(), "device_id=%{type: int}", dev->device_id);
#endif

    // Register device for shared ISR dispatch (config change cb optional)
    virtio_set_config_changed_callback(dev, NULL);

    // Allow multiple drivers to attach to same device id (e.g., input → kbd and
    // mouse wrappers)
    g_bool matched = false;
    for (uint32_t i = 0; i < g_driver_count; i++) {
      if (g_drivers[i]->device_id == dev->device_id) {
        matched = true;
        (void)g_drivers[i]->probe(dev);
      }
    }
    if (!matched) {
#if VIRTIO_DEBUG
      LOG_WARN(virtio_reg_log(), "no driver for device id %{type: int}",
               dev->device_id);
#endif
    }
  }
}

// Static enumeration for QEMU virt board: virtio-mmio at 0x10001000 + N*0x1000
// IRQ lines start at 1. This bypasses DTB while refactoring.
void virtio_bus_init_static(void) {
  uintptr_t base0 = 0x10001000;
  uint32_t step = 0x1000;
  // Only enumerate the 4 MMIO slots we map in main.c
  for (uint32_t i = 0; i < 4; i++) {
    uintptr_t base = base0 + (uintptr_t)(i * step);
    uint32_t irq = i + 1;
    virtio_device_t *dev = (virtio_device_t *)kalloc(sizeof(virtio_device_t));
    if (!dev)
      return;
    if (virtio_device_init(dev, base, irq) != 0)
      continue;
    virtio_set_config_changed_callback(dev, NULL);
    g_bool matched = false;
    for (uint32_t d = 0; d < g_driver_count; d++) {
      if (g_drivers[d]->device_id == dev->device_id) {
        matched = true;
        (void)g_drivers[d]->probe(dev);
      }
    }
    if (!matched) {
#if VIRTIO_DEBUG
      LOG_WARN(virtio_reg_log(), "no driver for device id %{type: int} at 0x%{type: hex}", dev->device_id, (uint64_t)base);
#endif
    }
  }
}

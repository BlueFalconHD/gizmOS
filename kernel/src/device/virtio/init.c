#include <device/virtio/virtio.h>
#include <device/virtio/drivers/block.h>
#include <device/virtio/drivers/input.h>

static int blk_probe_shim(virtio_device_t *dev) { return virtio_blk_probe(dev); }
static int input_probe_shim(virtio_device_t *dev) { return virtio_input_driver_probe(dev); }

static const virtio_driver_t g_blk_drv   = { .device_id = VIRTIO_DEVICE_BLOCK, .probe = blk_probe_shim };
static const virtio_driver_t g_input_drv = { .device_id = VIRTIO_DEVICE_INPUT, .probe = input_probe_shim };

void virtio_register_all_drivers(void) {
  virtio_register_driver(&g_blk_drv);
  virtio_register_driver(&g_input_drv);
}



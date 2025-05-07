//
// Simple 2-D VirtIO-GPU driver (VirtIO 1.1+, MMIO, control-queue only)
// – QEMU “virtio-gpu-device” on RISC-V ‘virt’ machine
//
#pragma once
#include "virtio_common.h"

/* ---------- GPU-specific -------------------------------------------------- */
#define VIRTIO_DEVICE_ID_GPU        16
#define VIRTIO_GPU_F_VIRGL          (1 << 0) /* we leave this *off*          */
#define VIRTIO_GPU_F_EDID           (1 << 1) /* leave off – no hot-plug yet  */
#define VIRTIO_F_VERSION_1          (1ULL << 32)

/* command IDs we actually issue (spec §6.1.3) */
#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO         0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D       0x0101
#define VIRTIO_GPU_CMD_SET_SCANOUT              0x0103
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH           0x0104
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D      0x0105
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING  0x0106

/* simple control-header (spec fig. 6.2) */
struct virtio_gpu_ctrl_hdr {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint8_t  _reserved;
    uint8_t  ring_idx;
    uint16_t _padding;
} __attribute__((packed));

/* framebuffer format we use (BGRA 8:8:8:8 u-norm) */
#define VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM  1

/* -------------------------------------------------------------------------- */
typedef struct {
    /* MMIO + common queue helpers */
    virtio_device_t  vdev;
    virtio_queue_t   q_ctrl;        /* queue 0 */

    /* current display (we only use scan-out 0) */
    uint32_t         width;
    uint32_t         height;
    uint32_t         resource_id;

    /* guest backing store */
    void            *fb_virt;       /* kernel-virtual address                */
    uint32_t         fb_bytes;      /* size of framebuffer in bytes          */
} virtio_gpu_t;

/* constructors / helpers */
RESULT_TYPE(virtio_gpu_t *) make_virtio_gpu(uint64_t base, uint32_t irq);
g_bool                virtio_gpu_init(virtio_gpu_t *g);
void                  virtio_gpu_handle_irq(virtio_gpu_t *g); /* ISR */

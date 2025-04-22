#pragma once
#include "virtio_common.h"
#include "virtio.h"

#define VIRTIO_MOUSE_EVENT_NUM  16   /* descriptors we keep posted          */

#define REL_X   0x00
#define REL_Y   0x01
#define REL_WHEEL 0x08

#define BTN_LEFT   0x110
#define BTN_RIGHT  0x111
#define BTN_MIDDLE 0x112

/** one‑byte DRIVER_OK packet for control queue */
struct virtio_mouse_status_pkt {
    uint16_t select;     /* VIRTIO_INPUT_CFG_ID_STATUS == 1  */
    uint16_t reserved;
    uint32_t size;       /* 1                                */
    uint8_t  data;       /* 1 == DRIVER_OK                   */
} __attribute__((packed));

typedef struct {
    /* base VirtIO wrapper */
    virtio_device_t vdev;

    /* queue 0 – incoming events */
    virtio_queue_t  q_events;
    struct virtio_input_event events[VIRTIO_MOUSE_EVENT_NUM];

    /* queue 1 – cfg / status */
    virtio_queue_t  q_ctl;
    struct virtio_mouse_status_pkt status_pkts[8];

    /* simple state tracker so higher layers can poll */
    int32_t rel_x;
    int32_t rel_y;
    int32_t wheel;
    uint8_t buttons;   /* bit0=L, bit1=R, bit2=M            */
} virtio_mouse_t;

RESULT_TYPE(virtio_mouse_t *) make_virtio_mouse(uint64_t base, uint32_t irq);
g_bool  virtio_mouse_init   (virtio_mouse_t *m);
void    virtio_mouse_handle_irq(virtio_mouse_t *m);

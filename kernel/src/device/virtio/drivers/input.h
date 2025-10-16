#pragma once

#include "../core/device.h"
#include "../core/spec.h"
#include <lib/types.h>
#include <stdint.h>

typedef void (*virtio_input_event_cb)(const struct virtio_input_event *ev,
                                      void *user);

typedef struct virtio_input_dev {
  virtio_device_t *vdev;
  struct virtq *q_events;
  struct virtq *q_ctl;
  struct virtio_input_event *events;
  uint16_t event_cap;
  virtio_input_event_cb on_event;
  void *user;

  // Optional secondary listener (e.g., mouse or keyboard) on the same input
  // device
  virtio_input_event_cb extra_cb;
  void *extra_user;

  // Unified input driver state
  uint16_t kbd_modifiers;
  int32_t  mouse_rel_x;
  int32_t  mouse_rel_y;
  int32_t  mouse_wheel;
  uint8_t  mouse_buttons;
} virtio_input_dev_t;

int virtio_input_probe(virtio_device_t *dev, uint16_t event_ring_size);
void virtio_input_set_callback(virtio_input_dev_t *inp,
                               virtio_input_event_cb cb, void *user);
int virtio_input_add_listener(virtio_input_dev_t *inp, virtio_input_event_cb cb,
                              void *user);
virtio_input_dev_t *virtio_input_get_for(virtio_device_t *dev);
// Access the singleton instance once probed
virtio_input_dev_t *virtio_input_get(void);

// Unified input driver: probe and install a single callback that
// handles both keyboard and mouse events for a VirtIO input device.
int virtio_input_driver_probe(virtio_device_t *dev);

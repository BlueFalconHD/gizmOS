#pragma once

#include <stdint.h>

// Limits kept conservative to simplify initial implementation
#define NOTIF_MAX_TYPE 64
#define NOTIF_QUEUE_SIZE 32

// Delivery flags
#define NOTIF_DFLAG_TRUNCATED 0x1u

typedef struct notif_msg {
  uint16_t type;
  uint16_t flags;     // delivery/runtime flags (e.g., TRUNCATED)
  uint32_t reserved;  // reserved for alignment/future
  void    *kbuf;      // kernel-owned buffer
  uint64_t len;       // payload length in bytes
  uint64_t id;        // unique id per proc
} notif_msg_t;

typedef struct notif_handler {
  uint64_t handler_va; // user function entry
  uint64_t arg_va;     // user-provided opaque pointer
  uint32_t flags;      // registration flags (unused for now)
  uint32_t id;         // registration id (monotonic per type)
} notif_handler_t;

typedef struct notif_ctx {
  uint64_t saved_epc;
  uint64_t saved_ra;
  uint64_t a[8];     // snapshot of a0..a7
  uint8_t  valid;    // non-zero if a handler is currently staged
} notif_ctx_t;

// Support limited nesting of notification handlers (e.g., reply during keypress).
// Depth includes the currently active handler. Depth 0 means no active handler.
#define NOTIF_MAX_NEST_DEPTH 5
typedef struct notif_ctx_stack {
  notif_ctx_t frames[NOTIF_MAX_NEST_DEPTH];
  uint8_t     depth; // 0..NOTIF_MAX_NEST_DEPTH
} notif_ctx_stack_t;


// Reserved notification type ids
// Keep small and stable; 0 is unused
#define NOTIF_TYPE_KEYPRESS 1

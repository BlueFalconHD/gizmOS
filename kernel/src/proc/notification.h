#pragma once

#include <lib/types.h>
#include <stdint.h>

struct proc;

#include "notification_types.h"

// Per-process initialization
void notification_init_proc(struct proc *p);

// Register/unregister a handler for a notification type.
// Returns registration id (>0) on success, 0 on failure.
uint32_t notification_register(struct proc *p,
                               uint16_t      type,
                               uint64_t      handler_va,
                               uint64_t      arg_va,
                               uint32_t      flags);

g_bool notification_unregister(struct proc *p, uint16_t type, uint32_t id);

// Enqueue a kernel-owned payload to a process.
// The payload is copied into a kalloc'ed buffer owned by the queue; caller's
// buffer is not retained after the call returns.
g_bool notification_post_copy(struct proc *p,
                              uint16_t     type,
                              const void  *data,
                              uint64_t     len,
                              uint16_t     flags);

// Pop next notification (internal, guarded by p->lock appropriately).
g_bool notification_pop(struct proc *p, notif_msg_t *out);

// Ensure per-process user executable stub and buffer are mapped and initialized.
g_bool notification_ensure_userbuf(struct proc *p);

// Delivery context helpers
void notif_ctx_clear(struct proc *p);
void notif_ctx_save_from_trapframe(struct proc *p);
void notif_ctx_restore_to_trapframe(struct proc *p);
// New helpers for nested delivery
g_bool notif_ctx_can_nest(struct proc *p);
void   notif_ctx_push_from_trapframe(struct proc *p);
void   notif_ctx_pop_restore_to_trapframe(struct proc *p);

// Constants for the injected return stub layout inside user buffer
#define NOTIF_STUB_OFFSET   0x0
#define NOTIF_STUB_SIZE     8  /* li a7, SYSCALL_NOTIF_DONE; ecall */
#define NOTIF_PAYLOAD_OFFSET (NOTIF_STUB_OFFSET + NOTIF_STUB_SIZE)



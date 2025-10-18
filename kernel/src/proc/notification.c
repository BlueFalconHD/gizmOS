#include "notification.h"
#include "buddy_allocator.h"
#include "notification_types.h"
#include <lib/kalloc.h>
#include <lib/memory.h>
#include <lib/print.h>
#include <lib/log.h>
#if NOTIF_DEBUG_LEVEL >= 1 || NOTIF_DEBUG_LEVEL >= 2
static inline log_t *notif_log() {
  static log_t *l = NULL;
  if (!l)
    l = g_log_create("proc", "notif");
  return l;
}
#endif
#include <lib/spinlock.h>
#include <lib/usermem.h>
#include <mem_layout.h>
#include <page_table.h>
#include <proc/process.h>

// Simple ring buffer kept inside proc_t; helpers here operate with p->lock held

static inline uint32_t inc_mod(uint32_t v, uint32_t m) { return (v + 1) % m; }

#ifndef NOTIF_DEBUG_LEVEL
#define NOTIF_DEBUG_LEVEL 0
#endif

void notification_init_proc(struct proc *p) {
  // Zero is already ensured by allocator/initialization, but be explicit.
  // Handlers table and queue indices live in proc_t; see process.h changes.
  (void)p;
}

uint32_t notification_register(struct proc *p, uint16_t type,
                               uint64_t handler_va, uint64_t arg_va,
                               uint32_t flags) {
  if (!p || type >= NOTIF_MAX_TYPE || handler_va == 0)
    return 0;
  acquire(&p->lock);
  notif_handler_t *h = &p->notif_handlers[type];
  h->handler_va = handler_va;
  h->arg_va = arg_va;
  h->flags = flags;
  h->id++;
  uint32_t id = h->id;
  release(&p->lock);
  return id;
}

g_bool notification_unregister(struct proc *p, uint16_t type, uint32_t id) {
  if (!p || type >= NOTIF_MAX_TYPE)
    return false;
  g_bool ok = false;
  acquire(&p->lock);
  notif_handler_t *h = &p->notif_handlers[type];
  if (h->id == id && h->handler_va != 0) {
    h->handler_va = 0;
    h->arg_va = 0;
    h->flags = 0;
    ok = true;
  }
  release(&p->lock);
  return ok;
}

g_bool notification_post_copy(struct proc *p, uint16_t type, const void *data,
                              uint64_t len, uint16_t flags) {
  if (!p || type >= NOTIF_MAX_TYPE)
    return false;

  void *kbuf = NULL;
  if (len > 0) {
    kbuf = kalloc(len);
    if (!kbuf)
      return false;
    memcpy(kbuf, data, len);
  }

  g_bool enq = false;
  acquire(&p->lock);

  uint32_t next_tail = inc_mod(p->notif_q_tail, NOTIF_QUEUE_SIZE);
  if (next_tail == p->notif_q_head) {
    // queue full: drop newest by default
    p->notif_stats_dropped++;
#if NOTIF_DEBUG_LEVEL >= 1
    LOG_WARN(notif_log(), "drop: pid=%{type: int} type=%{type: int} len=%{type: int}", p->pid, (int)type, (int)len);
#endif
  } else {
    notif_msg_t *m = &p->notif_queue[p->notif_q_tail];
    m->type = type;
    m->flags = flags;
    m->reserved = 0;
    m->kbuf = kbuf;
    m->len = len;
    m->id = ++p->notif_seq;
    p->notif_q_tail = next_tail;
    p->notif_pending = 1;
    enq = true;
#if NOTIF_DEBUG_LEVEL >= 2
    LOG_DEBUG(notif_log(), "enq: pid=%{type: int} id=%{type: int} type=%{type: int} len=%{type: int}", p->pid, (int)m->id, (int)m->type, (int)m->len);
#endif
  }

  release(&p->lock);
  if (!enq && kbuf)
    kfree(kbuf);
  return enq;
}

g_bool notification_pop(struct proc *p, notif_msg_t *out) {
  if (!p || !out)
    return false;
  g_bool ok = false;
  acquire(&p->lock);
  if (p->notif_q_head != p->notif_q_tail) {
    *out = p->notif_queue[p->notif_q_head];
    p->notif_q_head = inc_mod(p->notif_q_head, NOTIF_QUEUE_SIZE);
    ok = true;
#if NOTIF_DEBUG_LEVEL >= 2
    LOG_DEBUG(notif_log(), "pop: pid=%{type: int} id=%{type: int} type=%{type: int} len=%{type: int}", p->pid, (int)out->id, (int)out->type, (int)out->len);
#endif
  } else {
    p->notif_pending = 0;
  }
  release(&p->lock);
  return ok;
}

void notif_ctx_clear(struct proc *p) { p->notif_ctx.valid = 0; }

void notif_ctx_save_from_trapframe(struct proc *p) {
  p->notif_ctx.saved_epc = p->trapframe->epc;
  p->notif_ctx.a[0] = p->trapframe->a0;
  p->notif_ctx.a[1] = p->trapframe->a1;
  p->notif_ctx.a[2] = p->trapframe->a2;
  p->notif_ctx.a[3] = p->trapframe->a3;
  p->notif_ctx.a[4] = p->trapframe->a4;
  p->notif_ctx.a[5] = p->trapframe->a5;
  p->notif_ctx.a[6] = p->trapframe->a6;
  p->notif_ctx.a[7] = p->trapframe->a7;
  p->notif_ctx.valid = 1;
}

void notif_ctx_restore_to_trapframe(struct proc *p) {
  if (!p->notif_ctx.valid)
    return;
  p->trapframe->epc = p->notif_ctx.saved_epc;
  p->trapframe->a0 = p->notif_ctx.a[0];
  p->trapframe->a1 = p->notif_ctx.a[1];
  p->trapframe->a2 = p->notif_ctx.a[2];
  p->trapframe->a3 = p->notif_ctx.a[3];
  p->trapframe->a4 = p->notif_ctx.a[4];
  p->trapframe->a5 = p->notif_ctx.a[5];
  p->trapframe->a6 = p->notif_ctx.a[6];
  p->trapframe->a7 = p->notif_ctx.a[7];
  p->notif_ctx.valid = 0;
}

g_bool notification_ensure_userbuf(struct proc *p) {
  if (p->notif_userbuf_base != 0)
    return true;

  // Reserve a small user buffer region: map one page minimum for stub+payload,
  // can expand later if needed.
  const uint64_t base = NOTIF_BUF_BASE;
  const uint64_t size = NOTIF_BUF_SIZE;
  for (uint64_t off = 0; off < size; off += PAGE_SIZE) {
    void *pg = buddy_alloc_page();
    if (!pg)
      return false;
    memset(pg, 0, PAGE_SIZE);
    if (!map_page(p->pagetable, base + off, V2P((uint64_t)pg),
                  PTE_R | PTE_W | PTE_X | PTE_U | PTE_V)) {
      return false;
    }
  }

  p->notif_userbuf_base = base;
  p->notif_userbuf_size = size;

  // Install a minimal RISC-V user stub at base: `ecall` (to signal completion).
  // Encoding: 0x00000073 (ecall)
  uint32_t stub = 0x00000073u;
  if (!result_is_ok(copyout(p->pagetable, base + NOTIF_STUB_OFFSET, &stub,
                            sizeof(stub)))) {
    return false;
  }

  return true;
}

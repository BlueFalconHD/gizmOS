#include "spine.h"
#include "process.h"
#include "process_table.h"
#include "sleep.h"
#include <lib/kalloc.h>
#include <lib/log.h>
#include <lib/memory.h>
#include <lib/result.h>
#include <lib/spinlock.h>
#include <lib/str.h>
#include <lib/usermem.h>
#include <mem_layout.h>

#ifndef SPINE_DEBUG_LEVEL
#define SPINE_DEBUG_LEVEL 0
#endif

static inline log_t *spine_log() {
  static log_t *l = NULL;
  if (!l) {
    l = g_log_create("proc", "spine");
    #if SPINE_DEBUG_LEVEL >= 1
    g_log_set_level(l, LOG_LEVEL_DEBUG);
    #else
    g_log_set_level(l, LOG_LEVEL_INFO);
    #endif
  }
  return l;
}

static struct spinlock g_spine_registry_lock;
static uint8_t g_spine_registry_lock_inited = 0;

static uint64_t g_spine_token_seq = 0x9e3779b97f4a7c15ULL;
static struct spinlock g_spine_token_lock;
static uint8_t g_spine_token_lock_inited = 0;

static volatile uint64_t g_spine_ticks = 0;

static inline void spine_once_init() {
  if (!g_spine_registry_lock_inited) {
    initlock(&g_spine_registry_lock, "spine_registry");
    g_spine_registry_lock_inited = 1;
  }
  if (!g_spine_token_lock_inited) {
    initlock(&g_spine_token_lock, "spine_token");
    g_spine_token_lock_inited = 1;
  }
}

void spine_init_proc(struct proc *p) {
  spine_once_init();
  acquire(&g_spine_token_lock);
  uint64_t seq = g_spine_token_seq;
  g_spine_token_seq = g_spine_token_seq * 6364136223846793005ULL + 1ULL;
  release(&g_spine_token_lock);
  uint64_t mix = ((uint64_t)p->pid << 32) ^ ((uint64_t)(uintptr_t)p);
  p->spine_token = seq ^ mix;
  p->spine_service[0] = '\0';

  initlock(&p->spine_lock, "spine");
  p->spine_q_head = 0;
  p->spine_q_tail = 0;
  p->spine_pending = 0;
  p->spine_stats_dropped = 0;
  p->spine_wait_deadline = 0;
  p->spine_wait_chan = 0;
  for (uint32_t i = 0; i < SPINE_MSG_QUEUE_SIZE; i++) {
    p->spine_queue[i].kbuf = NULL;
    p->spine_queue[i].len = 0;
  }

  LOG_DEBUG(spine_log(),
            "spine_init_proc: pid=%{type: int} token=0x%{type: hex}",
            p->pid, p->spine_token);
}

void spine_on_exit(struct proc *p) {
  spine_once_init();
  if (p) {
    acquire(&p->spine_lock);
    for (uint32_t i = 0; i < SPINE_MSG_QUEUE_SIZE; i++) {
      if (p->spine_queue[i].kbuf) {
        kfree(p->spine_queue[i].kbuf);
        p->spine_queue[i].kbuf = NULL;
      }
      p->spine_queue[i].len = 0;
    }
    p->spine_q_head = 0;
    p->spine_q_tail = 0;
    p->spine_pending = 0;
    p->spine_wait_deadline = 0;
    release(&p->spine_lock);
  }

  acquire(&g_spine_registry_lock);
  p->spine_service[0] = '\0';
  release(&g_spine_registry_lock);

  LOG_DEBUG(spine_log(),
            "spine_on_exit: pid=%{type: int} token=0x%{type: hex}",
            p->pid, p->spine_token);
}

static proc_t *find_proc_by_pid_nolock(int pid) {
  for (uint8_t i = 0; i < NPROC; i++) {
    proc_t *q = &processes[i];
    acquire(&q->lock);
    g_bool match = (q->state != UNUSED && q->pid == pid);
    release(&q->lock);
    if (match) {
      return q;
    }
  }
  return NULL;
}

g_bool spine_service_advertise(struct proc *p, const char *name, uint32_t flags) {
  (void)flags;
  if (!p || !name) return false;
  size_t nlen = strlen(name);
  if (nlen == 0 || nlen >= SPINE_SERVICE_NAME_MAX) return false;

  spine_once_init();
  acquire(&g_spine_registry_lock);

  for (uint8_t i = 0; i < NPROC; i++) {
    proc_t *q = &processes[i];
    if (q == p) continue;
    acquire(&q->lock);
    g_bool taken = (q->state != UNUSED &&
                    q->spine_service[0] != '\0' &&
                    strcmp(q->spine_service, name));
    release(&q->lock);
    if (taken) {
#if SPINE_DEBUG_LEVEL >= 1
      LOG_DEBUG(spine_log(), "service name already taken: %{type: str}", name);
#endif
      release(&g_spine_registry_lock);
      return false;
    }
  }

  acquire(&p->lock);
  strncopy(p->spine_service, name, sizeof(p->spine_service));

  LOG_DEBUG(spine_log(),
            "spine_service_advertise: pid=%{type: int} name=%{type: str}",
            p->pid, p->spine_service);

  release(&p->lock);

  release(&g_spine_registry_lock);
  return true;
}

int64_t spine_service_lookup(const char *name, uint32_t flags) {
  (void)flags;
  if (!name) return -1;
  size_t nlen = strlen(name);
  if (nlen == 0 || nlen >= SPINE_SERVICE_NAME_MAX) return -1;

  spine_once_init();
  acquire(&g_spine_registry_lock);

  for (uint8_t i = 0; i < NPROC; i++) {
    proc_t *q = &processes[i];
    acquire(&q->lock);
    g_bool match = (q->state != UNUSED &&
                    q->spine_service[0] != '\0' &&
                    strcmp(q->spine_service, name));
    int pid = q->pid;
    release(&q->lock);
    if (match) {
      LOG_DEBUG(spine_log(),
                "spine_service_lookup: name=%{type: str} -> pid=%{type: int}",
                name, pid);

      release(&g_spine_registry_lock);
      return pid;
    }
  }

  release(&g_spine_registry_lock);
  return -1;
}

g_bool spine_msg_send(struct proc *src, int dest_pid,
                      const void *user_src, uint64_t size, uint32_t flags) {
  (void)flags;
  if (!src || !user_src) return false;

  const uint64_t header_size = sizeof(spine_wire_msg_t);
  if (size > (NOTIF_BUF_SIZE > header_size ? (NOTIF_BUF_SIZE - header_size) : 0)) {
    return false;
  }

  proc_t *dest = find_proc_by_pid_nolock(dest_pid);
  if (!dest) return false;

  uint64_t total = header_size + size;
  uint8_t *tmp = (uint8_t *)kalloc(total);
  if (!tmp) return false;

  spine_wire_msg_t *hdr = (spine_wire_msg_t *)tmp;
  hdr->sender_token = src->spine_token;
  hdr->message_size = (uint32_t)size;
  hdr->reserved = 0;

  if (!result_is_ok(copyin(src->pagetable, tmp + header_size, (uint64_t)user_src, size))) {
    LOG_WARN(spine_log(),
             "spine_msg_send: copyin failed: src_pid=%{type: int} dest_pid=%{type: int} size=%{type: int}",
             src->pid, dest_pid, (int)size);

    kfree(tmp);
    return false;
  }

  LOG_DEBUG(spine_log(),
            "spine_msg_send: src_pid=%{type: int} dest_pid=%{type: int} size=%{type: int}",
            src->pid, dest_pid, (int)size);

  acquire(&dest->spine_lock);
  if (dest->spine_pending >= SPINE_MSG_QUEUE_SIZE) {
    dest->spine_stats_dropped++;
    release(&dest->spine_lock);
    kfree(tmp);
    return false;
  }
  uint32_t idx = dest->spine_q_tail;
  dest->spine_queue[idx].kbuf = tmp;
  dest->spine_queue[idx].len = total;
  dest->spine_q_tail = (dest->spine_q_tail + 1u) % SPINE_MSG_QUEUE_SIZE;
  dest->spine_pending++;
  release(&dest->spine_lock);

  wakeup((void *)&dest->spine_wait_chan);
  return true;
}

static int spine_msg_recv_body(proc_t *dst,
                               void *user_dst, uint64_t cap,
                               uint64_t user_out_len_ptr,
                               uint64_t user_sender_token_ptr,
                               uint64_t timeout_ticks,
                               uint32_t flags) {
  if (!dst || !user_dst) return -1;
  if (cap == 0) return -1;

  uint64_t start = __atomic_load_n(&g_spine_ticks, __ATOMIC_RELAXED);
  uint64_t deadline = 0;
  if (timeout_ticks != 0) {
    deadline = start + timeout_ticks;
  }
  __atomic_store_n(&dst->spine_wait_deadline, deadline, __ATOMIC_RELAXED);

  for (;;) {
    acquire(&dst->spine_lock);

    if (dst->spine_pending > 0) {
      spine_msg_t m = dst->spine_queue[dst->spine_q_head];
      if (!m.kbuf || m.len < sizeof(spine_wire_msg_t)) {
        dst->spine_queue[dst->spine_q_head].kbuf = NULL;
        dst->spine_queue[dst->spine_q_head].len = 0;
        dst->spine_q_head = (dst->spine_q_head + 1u) % SPINE_MSG_QUEUE_SIZE;
        if (dst->spine_pending) dst->spine_pending--;
        release(&dst->spine_lock);
        if (m.kbuf) kfree(m.kbuf);
        continue;
      }

      const spine_wire_msg_t *hdr = (const spine_wire_msg_t *)m.kbuf;
      uint64_t payload_len = (uint64_t)hdr->message_size;
      uint64_t avail = m.len - (uint64_t)sizeof(spine_wire_msg_t);
      if (payload_len > avail) payload_len = avail;

      if (user_out_len_ptr != 0) {
        (void)copyout(dst->pagetable, user_out_len_ptr, &payload_len, sizeof(payload_len));
      }
      if (user_sender_token_ptr != 0) {
        uint64_t tok = hdr->sender_token;
        (void)copyout(dst->pagetable, user_sender_token_ptr, &tok, sizeof(tok));
      }

      if (cap < payload_len) {
        release(&dst->spine_lock);
        __atomic_store_n(&dst->spine_wait_deadline, 0, __ATOMIC_RELAXED);
        return -3;
      }

      const uint8_t *payload = (const uint8_t *)m.kbuf + sizeof(spine_wire_msg_t);
      if (!result_is_ok(copyout(dst->pagetable, (uint64_t)user_dst, (void *)payload, payload_len))) {
        release(&dst->spine_lock);
        __atomic_store_n(&dst->spine_wait_deadline, 0, __ATOMIC_RELAXED);
        return -1;
      }

      dst->spine_queue[dst->spine_q_head].kbuf = NULL;
      dst->spine_queue[dst->spine_q_head].len = 0;
      dst->spine_q_head = (dst->spine_q_head + 1u) % SPINE_MSG_QUEUE_SIZE;
      dst->spine_pending--;
      release(&dst->spine_lock);

      if (m.kbuf) kfree(m.kbuf);
      __atomic_store_n(&dst->spine_wait_deadline, 0, __ATOMIC_RELAXED);
      (void)flags;
      return 0;
    }

    if (dst->killed) {
      release(&dst->spine_lock);
      __atomic_store_n(&dst->spine_wait_deadline, 0, __ATOMIC_RELAXED);
      return -1;
    }

    if (deadline != 0) {
      uint64_t now = __atomic_load_n(&g_spine_ticks, __ATOMIC_RELAXED);
      if (now >= deadline) {
        release(&dst->spine_lock);
        if (user_out_len_ptr != 0) {
          uint64_t z = 0;
          (void)copyout(dst->pagetable, user_out_len_ptr, &z, sizeof(z));
        }
        __atomic_store_n(&dst->spine_wait_deadline, 0, __ATOMIC_RELAXED);
        return -2;
      }
    }

    sleep((void *)&dst->spine_wait_chan, &dst->spine_lock);
    // sleep returns with dst->spine_lock held.
    release(&dst->spine_lock);
  }
}

void spine_on_timer_tick(void) {
  uint64_t now = __atomic_add_fetch(&g_spine_ticks, 1, __ATOMIC_RELAXED);
  for (uint8_t i = 0; i < NPROC; i++) {
    proc_t *p = &processes[i];
    acquire(&p->lock);
    if (p->state == SLEEPING && p->chan == (void *)&p->spine_wait_chan) {
      uint64_t deadline = __atomic_load_n(&p->spine_wait_deadline, __ATOMIC_RELAXED);
      if (deadline != 0 && now >= deadline) {
        p->state = RUNNABLE;
      }
    }
    release(&p->lock);
  }
}

int spine_msg(struct proc *p, const spine_msg_args_t *uargs, uint64_t uargs_size) {
  if (!p || !uargs) return -1;
  if (uargs_size < sizeof(spine_msg_args_t)) return -1;

  spine_msg_args_t a;
  if (!result_is_ok(copyin(p->pagetable, &a, (uint64_t)uargs, sizeof(a)))) {
    return -1;
  }

  if ((a.flags & (SPINE_MSGF_SEND | SPINE_MSGF_RECV)) == 0) return -1;

  if (a.flags & SPINE_MSGF_SEND) {
    if (a.dest_pid < 0 || a.send_buf == 0) return -1;
    if (!spine_msg_send(p, (int)a.dest_pid, (const void *)a.send_buf, a.send_len, 0)) {
      return -1;
    }
  }

  if (a.flags & SPINE_MSGF_RECV) {
    if (a.recv_buf == 0) return -1;
    uint32_t rflags = a.flags;
    if ((rflags & SPINE_MSGF_RECV_BODY_ONLY) == 0) {
      // Default behavior is body-only; keep wire header internal.
      rflags |= SPINE_MSGF_RECV_BODY_ONLY;
    }
    return spine_msg_recv_body(p,
                               (void *)a.recv_buf, a.recv_cap,
                               a.recv_len_out,
                               a.sender_token_out,
                               a.timeout_ticks,
                               rflags);
  }

  return 0;
}

g_bool spine_get_seal(uint64_t token, spine_seal_t *out) {
  if (!out) return false;
  for (uint8_t i = 0; i < NPROC; i++) {
    proc_t *q = &processes[i];
    acquire(&q->lock);
    g_bool match = (q->state != UNUSED && q->spine_token == token);
    if (match) {
      out->token = token;
      out->pid = (uint32_t)q->pid;
      // Ensure NUL termination/padding
      memset(out->name, 0, sizeof(out->name));
      memset(out->service, 0, sizeof(out->service));
      if (q->name[0]) {
        strncopy(out->name, q->name, sizeof(out->name));
      }
      if (q->spine_service[0]) {
        strncopy(out->service, q->spine_service, sizeof(out->service));
      }
      release(&q->lock);
      return true;
    }
    release(&q->lock);
  }
  return false;
}

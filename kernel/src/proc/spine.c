#include "spine.h"
#include "notification.h"
#include "notification_types.h"
#include "process.h"
#include "process_table.h"
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

#if SPINE_DEBUG_LEVEL >= 1
static inline log_t *spine_log() {
  static log_t *l = NULL;
  if (!l) {
    l = g_log_create("proc", "spine");
  }
  return l;
}
#endif

// Registry lock to protect service name uniqueness and lookups.
static struct spinlock g_spine_registry_lock;
static uint8_t g_spine_registry_lock_inited = 0;

// Monotonic token sequence; mixed with pid and address for basic uniqueness.
static uint64_t g_spine_token_seq = 0x9e3779b97f4a7c15ULL;
static struct spinlock g_spine_token_lock;
static uint8_t g_spine_token_lock_inited = 0;

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
  // Generate a per-process token that is unlikely to collide:
  // combine a monotonic counter, pid, and the address of the proc struct.
  acquire(&g_spine_token_lock);
  uint64_t seq = g_spine_token_seq;
  g_spine_token_seq = g_spine_token_seq * 6364136223846793005ULL + 1ULL;
  release(&g_spine_token_lock);
  uint64_t mix = ((uint64_t)p->pid << 32) ^ ((uint64_t)(uintptr_t)p);
  p->spine_token = seq ^ mix;
  p->spine_service[0] = '\0';
}

void spine_on_exit(struct proc *p) {
  spine_once_init();
  acquire(&g_spine_registry_lock);
  p->spine_service[0] = '\0';
  release(&g_spine_registry_lock);
}

static proc_t *find_proc_by_pid_nolock(int pid) {
  for (uint8_t i = 0; i < NPROC; i++) {
    proc_t *q = &processes[i];
    // No lock for quick existence check; take the lock briefly to validate fields.
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
  // Validate name length and characters; enforce <= SPINE_SERVICE_NAME_MAX - 1.
  size_t nlen = strlen(name);
  if (nlen == 0 || nlen >= SPINE_SERVICE_NAME_MAX) return false;

  spine_once_init();
  acquire(&g_spine_registry_lock);

  // Uniqueness check across all processes.
  for (uint8_t i = 0; i < NPROC; i++) {
    proc_t *q = &processes[i];
    if (q == p) continue;
    acquire(&q->lock);
    g_bool taken = (q->state != UNUSED && q->spine_service[0] != '\0' &&
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

  // Assign name to this process.
  acquire(&p->lock);
  strncopy(p->spine_service, name, sizeof(p->spine_service));
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
    g_bool match = (q->state != UNUSED && q->spine_service[0] != '\0' &&
                    strcmp(q->spine_service, name));
    int pid = q->pid;
    release(&q->lock);
    if (match) {
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

  // Upper bound so we do not exceed the per-proc notification user buffer.
  const uint64_t header_size = sizeof(spine_wire_msg_t);
  const uint64_t max_payload = (NOTIF_BUF_SIZE - NOTIF_PAYLOAD_OFFSET);
  if (size > (max_payload > header_size ? (max_payload - header_size) : 0)) {
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
    kfree(tmp);
    return false;
  }

  g_bool ok = notification_post_copy(dest, NOTIF_TYPE_SPINE_MESSAGE, tmp, total, 0);
  kfree(tmp);
  return ok;
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



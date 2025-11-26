#include "syscall.h"
#include "lib/log.h"
#include "lib/print.h"
#include "lib/result.h"
#include "lib/usermem.h"
#include "lifecycle.h"
#include "proc/process_table.h"
#include "proc/memory.h"
#include <fs/objectfs/objfs.h>
#include <fs/objectfs/objfs_format.h>
#include <fs/objectfs/objfs.h>
#include "proc/notification.h"
#include "proc/spine.h"
#include <lib/str.h>
#include <lib/memory.h>

#define SYSCALL_DEBUG 10

// Subobject listing helper callbacks (for handle API)
typedef struct { int found; } has_subobject_ctx_t;
static void has_subobject_cb(const char *name, uint8_t kind, uint64_t cid, void *arg) {
  (void)name; (void)kind; (void)cid;
  has_subobject_ctx_t *c = (has_subobject_ctx_t *)arg;
  c->found = 1;
}
typedef struct { uint64_t cnt; } count_subobject_ctx_t;
static void count_subobject_cb(const char *name, uint8_t kind, uint64_t cid, void *arg) {
  (void)name; (void)kind; (void)cid;
  count_subobject_ctx_t *c = (count_subobject_ctx_t *)arg;
  c->cnt++;
}
typedef struct { uint64_t want; uint64_t cur; uint64_t out; int found; } nth_subobject_ctx_t;
static void nth_subobject_cb(const char *name, uint8_t kind, uint64_t cid, void *arg) {
  (void)name; (void)kind;
  nth_subobject_ctx_t *c = (nth_subobject_ctx_t *)arg;
  if (c->found) return;
  if (c->cur == c->want) { c->out = cid; c->found = 1; return; }
  c->cur++;
}

static inline log_t *syscall_log() {
  static log_t *l = NULL;
  if (!l) {
    l = g_log_create("proc", "syscall");
#ifdef G_DEBUG
#ifdef SYSCALL_DEBUG
    g_log_set_level(l, LOG_LEVEL_DEBUG);
#endif
    g_log_set_level(l, LOG_LEVEL_INFO);
#else
    g_log_set_level(l, LOG_LEVEL_INFO);
#endif
  }
  return l;
}

syscall_err_t syscall_dispatch(proc_t *p, syscall_num_t num) {
  for (uint64_t i = 0; i < sizeof(syscall_table) / sizeof(syscall_entry_t);
       i++) {
    syscall_entry_t se = syscall_table[i];
    if (num == se.call_number) {
      LOG_DEBUG(syscall_log(),
               "syscall dispatched from %{type: int}: %{type: str} (0x%{type: hex})",
               p->pid, se.desc, num);
      return se.handler(p, num);
    }
  }

  return SYSCALL_ERR_NONEXISTENT_CALLNUM;
}

syscall_err_t syscall_handle_lifecycle(proc_t *p, syscall_num_t num) {
  switch (num) {
  case SYSCALL_NUM_EXIT:
    exit((uint64_t)(int)p->trapframe->a0);
    return SYSCALL_ERR_NONE;
    break;
  case SYSCALL_NUM_SPAWN: {
    // a0 = user path (8.3 or objectfs root entry), a1 = user name (optional, may be 0)
    result_t rpath = copyinstr(p->pagetable, p->trapframe->a0, 256);
    if (!result_is_ok(rpath)) {
      p->trapframe->a0 = (uint64_t)-1;
      return SYSCALL_ERR_NONE;
    }
    char *kpath = (char *)result_unwrap(rpath);
    char *kname = NULL;
    if (p->trapframe->a1) {
      result_t rname = copyinstr(p->pagetable, p->trapframe->a1, 128);
      if (!result_is_ok(rname)) {
        kfree(kpath);
        p->trapframe->a0 = (uint64_t)-1;
        return SYSCALL_ERR_NONE;
      }
      kname = (char *)result_unwrap(rname);
    }
    result_t rp = proc_from_vessel_path(kpath, kname ? kname : kpath);
    kfree(kpath);
    if (kname) kfree(kname);
    if (!result_is_ok(rp)) {
      p->trapframe->a0 = (uint64_t)-1;
      return SYSCALL_ERR_NONE;
    }
    proc_t *child = (proc_t *)result_unwrap(rp);
    acquire(&wait_lock);
    child->parent = p;
    release(&wait_lock);
    p->trapframe->a0 = (uint64_t)child->pid;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_WAIT: {
    uint64_t status_addr = p->trapframe->a0;
    uint64_t rpid = wait(status_addr);
    p->trapframe->a0 = rpid;
    return SYSCALL_ERR_NONE;
  }
  default:
    return SYSCALL_ERR_NONEXISTENT_CALLNUM;
    break;
  }
};

syscall_err_t syscall_handle_spine(proc_t *p, syscall_num_t num) {
  switch (num) {
  case SYSCALL_NUM_SPINE_MSG_SEND: {
    // a0=dest_pid, a1=user msg ptr, a2=msg size, a3=flags
    int dest_pid = (int)p->trapframe->a0;
    uint64_t uptr = p->trapframe->a1;
    uint64_t n = p->trapframe->a2;
    uint32_t flags = (uint32_t)p->trapframe->a3;
    g_bool ok = spine_msg_send(p, dest_pid, (const void *)uptr, n, flags);
    p->trapframe->a0 = ok ? 0 : (uint64_t)-1;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_SPINE_SERVICE_ADVERTISE: {
    // a0=user name*, a1=flags
    result_t rname = copyinstr(p->pagetable, p->trapframe->a0, SPINE_SERVICE_NAME_MAX);
    if (!result_is_ok(rname)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    char *kname = (char *)result_unwrap(rname);
    uint32_t flags = (uint32_t)p->trapframe->a1;
    g_bool ok = spine_service_advertise(p, kname, flags);
    kfree(kname);
    p->trapframe->a0 = ok ? 0 : (uint64_t)-1;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_SPINE_SERVICE_LOOKUP: {
    // a0=user name*, a1=flags -> returns pid or -1
    result_t rname = copyinstr(p->pagetable, p->trapframe->a0, SPINE_SERVICE_NAME_MAX);
    if (!result_is_ok(rname)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    char *kname = (char *)result_unwrap(rname);
    uint32_t flags = (uint32_t)p->trapframe->a1;
    int64_t pid = spine_service_lookup(kname, flags);
    kfree(kname);
    p->trapframe->a0 = pid;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_SPINE_GET_SEAL: {
    // a0=token, a1=user out ptr, a2=out size
    uint64_t token = p->trapframe->a0;
    uint64_t outva = p->trapframe->a1;
    uint64_t outsz = p->trapframe->a2;
    spine_seal_t seal;
    if (!spine_get_seal(token, &seal)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    uint64_t n = (outsz < sizeof(seal)) ? outsz : sizeof(seal);
    if (!result_is_ok(copyout(p->pagetable, outva, &seal, n))) {
      p->trapframe->a0 = (uint64_t)-1;
      return SYSCALL_ERR_NONE;
    }
    p->trapframe->a0 = 0;
    return SYSCALL_ERR_NONE;
  }
  default:
    return SYSCALL_ERR_NONEXISTENT_CALLNUM;
  }
}

// typedef enum syscall_num {
//   SYSCALL_NUM_EXIT = 0x02,

//   SYSCALL_NUM_PRINT_INT = 0x10,
//   SYSCALL_NUM_PRINT_STR = 0x11,

//   SYSCALL_NUM_NOTIF_REGISTER = 0x90,
//   SYSCALL_NUM_NOTIF_UNREGISTER = 0x91,
//   SYSCALL_NUM_NOTIF_DONE = 0x100,
// } syscall_num_t;

syscall_err_t syscall_handle_notification(proc_t *p, syscall_num_t num) {
  if (num == SYSCALL_NUM_NOTIF_REGISTER) {
    // a0=type, a1=handler, a2=arg, a3=flags
    uint16_t type = (uint16_t)p->trapframe->a0;
    uint64_t handler = p->trapframe->a1;
    uint64_t arg = p->trapframe->a2;
    uint32_t flags = (uint32_t)p->trapframe->a3;
    uint32_t id = notification_register(p, type, handler, arg, flags);
    p->trapframe->a0 = id; // return id
    return SYSCALL_ERR_NONE;
  } else if (num == SYSCALL_NUM_NOTIF_UNREGISTER) {
    uint16_t type = (uint16_t)p->trapframe->a0;
    uint32_t id = (uint32_t)p->trapframe->a1;
    g_bool ok = notification_unregister(p, type, id);
    p->trapframe->a0 = ok ? 0 : (uint64_t)-1;
    return SYSCALL_ERR_NONE;
  } else if (num == SYSCALL_NUM_NOTIF_DONE) {

    LOG_DEBUG(syscall_log(),
             "notif.done(): pid=%{type: int} valid(before)=%{type: int} "
             "(pending=%{type: int} head=%{type: int} tail=%{type: int})",
             p->pid, (int)p->notif_ctx.valid, (int)p->notif_pending,
             (int)p->notif_q_head, (int)p->notif_q_tail);

    /*
     * Completion of a notification handler:
     * If nested handlers are active, pop to the previous handler context;
     * otherwise restore the original user context.
     */
    if (p->notif_stack.depth > 0) {
      notif_ctx_pop_restore_to_trapframe(p);
    } else {
      notif_ctx_restore_to_trapframe(p);
    }

    LOG_DEBUG(syscall_log(),
             "notif.done(): pid=%{type: int} valid(after)=%{type: int} "
             "(pending=%{type: int} head=%{type: int} tail=%{type: int})",
             p->pid, (int)p->notif_ctx.valid, (int)p->notif_pending,
             (int)p->notif_q_head, (int)p->notif_q_tail);

    return SYSCALL_ERR_NONE;
  } else {
    return SYSCALL_ERR_NONEXISTENT_CALLNUM;
  }
};

syscall_err_t syscall_handle_work(proc_t *p, syscall_num_t num) {
  if (num == SYSCALL_NUM_PRINT_INT) {
    return SYSCALL_ERR_NONE;
  } else if (num == SYSCALL_NUM_PRINT_STR) {
    result_t rstr = copyinstr(p->pagetable, p->trapframe->a0, 256);
    if (!result_is_ok(rstr)) {
      LOG_WARN(syscall_log(),
               "sys_print_str: failed to copy string from user memory");
      p->trapframe->a0 = (uint64_t)-1;
      return SYSCALL_ERR_NONE;
    }
    char *ustr = (char *)result_unwrap(rstr);

    printf("%{type: str}", PRINT_FLAG_BOTH, ustr);
    return SYSCALL_ERR_NONE;
  } else {
    return SYSCALL_ERR_NONEXISTENT_CALLNUM;
  }

  return SYSCALL_ERR_NONE;
};

syscall_err_t syscall_handle_memory(proc_t *p, syscall_num_t num) {
  if (num != SYSCALL_NUM_SBRK)
    return SYSCALL_ERR_NONEXISTENT_CALLNUM;

  int64_t incr = (int64_t)p->trapframe->a0;
  uint64_t old = p->sz;

  if (incr == 0) {
    p->trapframe->a0 = old;
    return SYSCALL_ERR_NONE;
  }

  if (incr > 0) {
    uint64_t new_end = old + (uint64_t)incr;
    if (new_end < old)
      goto fail;
    if (p->stack_base && new_end > p->stack_base)
      goto fail;
    if (!uvmalloc(p, old, new_end))
      goto fail;
    p->trapframe->a0 = old;
    return SYSCALL_ERR_NONE;
  } else {
    uint64_t dec = (uint64_t)(-incr);
    if (dec > old)
      goto fail;
    uint64_t new_end = old - dec;
    if (new_end < p->heap_base)
      goto fail;
    if (!uvmdealloc(p, old, new_end))
      goto fail;
    p->trapframe->a0 = old;
    return SYSCALL_ERR_NONE;
  }

fail:
  p->trapframe->a0 = (uint64_t)-1;
  return SYSCALL_ERR_NONE;
}

// Helpers for OBJ_LIST_SUBOBJECTS
typedef struct {
  proc_t *p;
  uint64_t dst;
  uint64_t cap;
  uint64_t wrote;
} subobject_emit_ctx_t;
typedef struct __attribute__((packed)) {
  uint8_t  name_len;
  uint8_t  type;
  uint16_t _pad;
  uint64_t id;
  char     name[64];
} objdirent_user_t;
static void emit_subobject_to_user(const char *name, uint8_t kind, uint64_t id, void *arg) {
  subobject_emit_ctx_t *ctx = (subobject_emit_ctx_t *)arg;
  objdirent_user_t de;
  size_t n = 0;
  while (name[n] && n < 64) n++;
  de.name_len = (uint8_t)n;
  de.type = kind;
  de._pad = 0;
  de.id = id;
  for (size_t i = 0; i < 64; i++) de.name[i] = (i < n) ? name[i] : '\0';
  if (ctx->wrote + sizeof(de) > ctx->cap)
    return;
  uint64_t dstva = ctx->dst + ctx->wrote;
  if (result_is_ok(copyout(ctx->p->pagetable, dstva, &de, sizeof(de))))
    ctx->wrote += sizeof(de);
}

// Helpers for OBJ_ATTR_LIST
typedef struct {
  proc_t *p;
  uint64_t dst;
  uint64_t cap;
  uint64_t wrote;
} attr_emit_ctx_t;
typedef struct __attribute__((packed)) {
  uint8_t  key_len;
  uint8_t  type;
  uint16_t _pad;
  char     key[64];
} objattr_user_t;
static void emit_attr_to_user(const char *key, uint8_t type, void *arg) {
  attr_emit_ctx_t *ctx = (attr_emit_ctx_t *)arg;
  objattr_user_t ae;
  size_t n = 0;
  while (key[n] && n < 64) n++;
  ae.key_len = (uint8_t)n;
  ae.type = type;
  ae._pad = 0;
  for (size_t i = 0; i < 64; i++) ae.key[i] = (i < n) ? key[i] : '\0';
  if (ctx->wrote + sizeof(ae) > ctx->cap)
    return;
  uint64_t dstva = ctx->dst + ctx->wrote;
  if (result_is_ok(copyout(ctx->p->pagetable, dstva, &ae, sizeof(ae))))
    ctx->wrote += sizeof(ae);
}

syscall_err_t syscall_handle_fs(proc_t *p, syscall_num_t num) {
  switch (num) {
  case SYSCALL_NUM_OBJH_ID_AT: {
    // a0=user path
    result_t rpath = copyinstr(p->pagetable, p->trapframe->a0, 256);
    if (!result_is_ok(rpath)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    char *kpath = (char *)result_unwrap(rpath);
    uint64_t id = 0;
    result_t r = objfs_lookup_path(kpath, &id);
    kfree(kpath);
    p->trapframe->a0 = result_is_ok(r) ? id : (uint64_t)-1;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJH_OPEN: {
    // a0=obj_id, a1=flags
    uint64_t id = p->trapframe->a0;
    uint32_t flags = (uint32_t)p->trapframe->a1;
    int slot = -1;
    for (int i = 0; i < PROC_MAX_OBJH; i++) {
      if (p->objh_ids[i] == (uint64_t)-1) { slot = i; break; }
    }
    if (slot < 0) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    p->objh_ids[slot] = id;
    p->objh_flags[slot] = flags;
    p->trapframe->a0 = (uint64_t)slot;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJH_CLOSE: {
    // a0=handle
    int h = (int)p->trapframe->a0;
    if (h < 0 || h >= PROC_MAX_OBJH) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    p->objh_ids[h] = (uint64_t)-1;
    p->objh_flags[h] = 0;
    p->trapframe->a0 = 0;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJH_HAS_SUBS: {
    // a0=handle
    int h = (int)p->trapframe->a0;
    if (h < 0 || h >= PROC_MAX_OBJH || p->objh_ids[h] == (uint64_t)-1) {
      p->trapframe->a0 = 0; return SYSCALL_ERR_NONE;
    }
    uint64_t id = p->objh_ids[h];
    // count quickly: we can stop at first subobject
    has_subobject_ctx_t hctx = {.found = 0};
    objfs_list_subobjects(id, has_subobject_cb, &hctx);
    p->trapframe->a0 = hctx.found ? 1 : 0;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJH_SUBS_COUNT: {
    // a0=handle
    int h = (int)p->trapframe->a0;
    if (h < 0 || h >= PROC_MAX_OBJH || p->objh_ids[h] == (uint64_t)-1) {
      p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE;
    }
    uint64_t id = p->objh_ids[h];
    count_subobject_ctx_t cctx = {.cnt = 0};
    objfs_list_subobjects(id, count_subobject_cb, &cctx);
    p->trapframe->a0 = cctx.cnt;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJH_SUB_AT: {
    // a0=handle, a1=index
    int h = (int)p->trapframe->a0;
    uint64_t idx = p->trapframe->a1;
    if (h < 0 || h >= PROC_MAX_OBJH || p->objh_ids[h] == (uint64_t)-1) {
      p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE;
    }
    uint64_t id = p->objh_ids[h];
    nth_subobject_ctx_t nctx = {.want = idx, .cur = 0, .out = (uint64_t)-1, .found = 0};
    objfs_list_subobjects(id, nth_subobject_cb, &nctx);
    p->trapframe->a0 = nctx.found ? nctx.out : (uint64_t)-1;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJH_OPEN_AT: {
    // a0=user path, a1=flags
    result_t rpath = copyinstr(p->pagetable, p->trapframe->a0, 256);
    if (!result_is_ok(rpath)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    char *kpath = (char *)result_unwrap(rpath);
    uint64_t id = 0;
    result_t r = objfs_lookup_path(kpath, &id);
    kfree(kpath);
    if (!result_is_ok(r)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    int slot = -1;
    for (int i = 0; i < PROC_MAX_OBJH; i++) {
      if (p->objh_ids[i] == (uint64_t)-1) { slot = i; break; }
    }
    if (slot < 0) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    p->objh_ids[slot] = id;
    p->objh_flags[slot] = (uint32_t)p->trapframe->a1;
    p->trapframe->a0 = (uint64_t)slot;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJH_STAT: {
    // a0=handle, a1=user objfs_stat_t*
    int h = (int)p->trapframe->a0;
    if (h < 0 || h >= PROC_MAX_OBJH || p->objh_ids[h] == (uint64_t)-1) {
      p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE;
    }
    uint64_t id = p->objh_ids[h];
    objfs_stat_t st;
    result_t r = objfs_object_stat(id, &st);
    if (!result_is_ok(r)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    if (!result_is_ok(copyout(p->pagetable, p->trapframe->a1, &st, sizeof(st)))) {
      p->trapframe->a0 = (uint64_t)-1;
      return SYSCALL_ERR_NONE;
    }
    p->trapframe->a0 = 0;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJH_LIST_SUBOBJECTS: {
    // a0=handle, a1=user buf, a2=cap
    int h = (int)p->trapframe->a0;
    if (h < 0 || h >= PROC_MAX_OBJH || p->objh_ids[h] == (uint64_t)-1) {
      p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE;
    }
    uint64_t id = p->objh_ids[h];
    subobject_emit_ctx_t ctx = {.p = p, .dst = p->trapframe->a1, .cap = p->trapframe->a2, .wrote = 0};
    result_t r = objfs_list_subobjects(id, emit_subobject_to_user, &ctx);
    if (!result_is_ok(r)) p->trapframe->a0 = (uint64_t)-1;
    else p->trapframe->a0 = ctx.wrote;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJH_READ: {
    // a0=handle, a1=user dst, a2=offset, a3=nbytes
    int h = (int)p->trapframe->a0;
    if (h < 0 || h >= PROC_MAX_OBJH || p->objh_ids[h] == (uint64_t)-1) {
      p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE;
    }
    uint64_t id = p->objh_ids[h];
    uint64_t dst = p->trapframe->a1;
    uint64_t off = p->trapframe->a2;
    uint64_t n   = p->trapframe->a3;
    size_t chunk = (n > 4096) ? 4096 : (size_t)n;
    uint8_t *kbuf = (uint8_t *)kalloc(chunk);
    if (!kbuf) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    size_t outn = 0;
    result_t r = objfs_read(id, off, kbuf, chunk, &outn);
    if (!result_is_ok(r)) { kfree(kbuf); p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    if (!result_is_ok(copyout(p->pagetable, dst, kbuf, outn))) {
      kfree(kbuf); p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE;
    }
    kfree(kbuf);
    p->trapframe->a0 = outn;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJH_WRITE: {
    // a0=handle, a1=user src, a2=offset, a3=nbytes
    int h = (int)p->trapframe->a0;
    if (h < 0 || h >= PROC_MAX_OBJH || p->objh_ids[h] == (uint64_t)-1) {
      p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE;
    }
    uint64_t id = p->objh_ids[h];
    uint64_t src = p->trapframe->a1;
    uint64_t off = p->trapframe->a2;
    uint64_t n   = p->trapframe->a3;
    if (n > 4096) n = 4096;
    uint8_t *kbuf = (uint8_t *)kalloc((size_t)n);
    if (!kbuf) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    if (!result_is_ok(copyin(p->pagetable, kbuf, src, (size_t)n))) {
      kfree(kbuf); p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE;
    }
    size_t outn = 0;
    result_t r = objfs_write_bytes(id, off, kbuf, (size_t)n, &outn);
    kfree(kbuf);
    p->trapframe->a0 = result_is_ok(r) ? outn : (uint64_t)-1;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJH_SEEK: {
    // a0=handle, a1=off, a2=whence (0=SET,1=CUR,2=END)
    int h = (int)p->trapframe->a0;
    if (h < 0 || h >= PROC_MAX_OBJH || p->objh_ids[h] == (uint64_t)-1) {
      p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE;
    }
    // Compatibility: store per-handle offset in objh_flags high bits is messy; defer until unified descriptors
    // For now, SEEK only validates and echoes off for CUR=SET behavior
    uint64_t off = p->trapframe->a1;
    uint64_t whence = p->trapframe->a2;
    if (whence > 2) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    // no stateful offset yet; return requested off
    p->trapframe->a0 = off;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJH_ATTR_GET: {
    // a0=handle, a1=user key*, a2=user out struct*, a3=user str buf (opt), a4=cap
    int h = (int)p->trapframe->a0;
    if (h < 0 || h >= PROC_MAX_OBJH || p->objh_ids[h] == (uint64_t)-1)
      { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    result_t rkey = copyinstr(p->pagetable, p->trapframe->a1, 128);
    if (!result_is_ok(rkey)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    char *kkey = (char *)result_unwrap(rkey);
    uint64_t id = p->objh_ids[h];
    uint64_t outp = p->trapframe->a2;
    uint64_t strp = p->trapframe->a3;
    uint64_t cap  = p->trapframe->a4;
    objfs_attr_value_t av;
    result_t r = objfs_attr_get(id, kkey, &av);
    if (!result_is_ok(r)) { kfree(kkey); p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    if (!result_is_ok(copyout(p->pagetable, outp, &av, sizeof(av)))) {
      kfree(kkey); p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE;
    }
    if (av.type == OBJFS_ATTR_V_STR && strp && cap > 0) {
      size_t outlen = 0;
      char *tmp = (char *)kalloc(cap);
      if (!tmp) { kfree(kkey); p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
      result_t rg = objfs_attr_get_str(id, kkey, tmp, cap, &outlen);
      if (result_is_ok(rg)) {
        if (result_is_ok(copyout(p->pagetable, strp, tmp, outlen + 1))) {
          p->trapframe->a0 = outlen;
        } else {
          p->trapframe->a0 = (uint64_t)-1;
        }
      } else {
        p->trapframe->a0 = (uint64_t)-1;
      }
      kfree(tmp); kfree(kkey);
      return SYSCALL_ERR_NONE;
    }
    kfree(kkey);
    p->trapframe->a0 = 0;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJH_ATTR_LIST: {
    // a0=handle, a1=user buf, a2=cap
    int h = (int)p->trapframe->a0;
    if (h < 0 || h >= PROC_MAX_OBJH || p->objh_ids[h] == (uint64_t)-1) {
      p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE;
    }
    uint64_t id = p->objh_ids[h];
    attr_emit_ctx_t ctx = {.p = p, .dst = p->trapframe->a1, .cap = p->trapframe->a2, .wrote = 0};
    result_t r = objfs_list_attrs(id, emit_attr_to_user, &ctx);
    p->trapframe->a0 = result_is_ok(r) ? ctx.wrote : (uint64_t)-1;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJH_DESC: {
    // a0=handle, a1=user out struct
    int h = (int)p->trapframe->a0;
    if (h < 0 || h >= PROC_MAX_OBJH || p->objh_ids[h] == (uint64_t)-1) {
      p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE;
    }
    uint64_t id = p->objh_ids[h];
    typedef struct __attribute__((packed)) {
      uint64_t id;
      uint16_t mode;
      uint8_t  kind;
      uint8_t  flags;
      uint32_t uid;
      uint32_t gid;
      uint32_t nlink;
      uint64_t size;
      uint64_t atime, mtime, ctime;
      uint64_t target_id;
    } objfs_user_desc_t;
    objfs_object_disk_t d;
    result_t r = objfs_read_descriptor(id, &d);
    if (!result_is_ok(r)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    objfs_user_desc_t ud = {
        .id = d.id, .mode = d.mode, .kind = d.kind, .flags = d.flags,
        .uid = d.uid, .gid = d.gid, .nlink = d.nlink, .size = d.size,
        .atime = d.atime, .mtime = d.mtime, .ctime = d.ctime,
        .target_id = d.target_id};
    if (!result_is_ok(copyout(p->pagetable, p->trapframe->a1, &ud, sizeof(ud)))) {
      p->trapframe->a0 = (uint64_t)-1;
      return SYSCALL_ERR_NONE;
    }
    p->trapframe->a0 = 0;
    return SYSCALL_ERR_NONE;
  }
  // Note: objh_get_id is implemented by objh_desc in userspace wrapper; no new kernel call needed.
  case SYSCALL_NUM_OBJH_CREATE: {
    // a0=parent handle, a1=user name*, a2=mode, a3=kind
    int ph = (int)p->trapframe->a0;
    if (ph < 0 || ph >= PROC_MAX_OBJH || p->objh_ids[ph] == (uint64_t)-1) {
      p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE;
    }
    result_t rname = copyinstr(p->pagetable, p->trapframe->a1, 128);
    if (!result_is_ok(rname)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    char *kname = (char *)result_unwrap(rname);
    uint64_t parent = p->objh_ids[ph];
    uint16_t mode = (uint16_t)p->trapframe->a2;
    uint8_t  kind = (uint8_t)p->trapframe->a3;
    uint64_t new_id = 0;
    result_t r = objfs_create(parent, kname, mode, kind, &new_id);
    kfree(kname);
    if (!result_is_ok(r)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    // open handle for new object
    int slot = -1;
    for (int i = 0; i < PROC_MAX_OBJH; i++) if (p->objh_ids[i] == (uint64_t)-1) { slot = i; break; }
    if (slot < 0) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    p->objh_ids[slot] = new_id; p->objh_flags[slot] = 0;
    p->trapframe->a0 = (uint64_t)slot;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJH_SET_ATTR: {
    // a0=handle, a1=user key*, a2=type, a3=user value ptr, a4=cap_or_size
    int h = (int)p->trapframe->a0;
    if (h < 0 || h >= PROC_MAX_OBJH || p->objh_ids[h] == (uint64_t)-1)
      { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    result_t rkey = copyinstr(p->pagetable, p->trapframe->a1, 128);
    if (!result_is_ok(rkey)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    char *kkey = (char *)result_unwrap(rkey);
    uint64_t id = p->objh_ids[h];
    uint8_t type = (uint8_t)p->trapframe->a2;
    result_t rr = RESULT_FAILURE(RESULT_INVALID);
    if (type == OBJFS_ATTR_V_STR) {
      uint64_t cap = p->trapframe->a4;
      if (cap == 0 || cap > 1024) cap = 1024;
      char *tmp = (char *)kalloc((size_t)cap);
      if (!tmp) { kfree(kkey); p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
      if (!result_is_ok(copyin(p->pagetable, tmp, p->trapframe->a3, (size_t)cap))) {
        kfree(tmp); kfree(kkey); p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE;
      }
      rr = objfs_set_attr_str(id, kkey, tmp);
      kfree(tmp);
    } else if (type == OBJFS_ATTR_V_INT) {
      int64_t v = 0;
      if (!result_is_ok(copyin(p->pagetable, &v, p->trapframe->a3, sizeof(v)))) {
        kfree(kkey); p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE;
      }
      rr = objfs_set_attr_int(id, kkey, v);
    } else if (type == OBJFS_ATTR_V_BOOL) {
      uint8_t b = 0;
      if (!result_is_ok(copyin(p->pagetable, &b, p->trapframe->a3, sizeof(b)))) {
        kfree(kkey); p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE;
      }
      rr = objfs_set_attr_bool(id, kkey, b);
    }
    kfree(kkey);
    p->trapframe->a0 = result_is_ok(rr) ? 0 : (uint64_t)-1;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJH_LINK: {
    // a0=parent handle, a1=user name*, a2=target handle
    int ph = (int)p->trapframe->a0;
    int th = (int)p->trapframe->a2;
    if (ph < 0 || ph >= PROC_MAX_OBJH || p->objh_ids[ph] == (uint64_t)-1) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    if (th < 0 || th >= PROC_MAX_OBJH || p->objh_ids[th] == (uint64_t)-1) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    result_t rname = copyinstr(p->pagetable, p->trapframe->a1, 128);
    if (!result_is_ok(rname)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    char *kname = (char *)result_unwrap(rname);
    result_t r = objfs_link(p->objh_ids[ph], kname, p->objh_ids[th]);
    kfree(kname);
    p->trapframe->a0 = result_is_ok(r) ? 0 : (uint64_t)-1;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJH_UNLINK: {
    // a0=parent handle, a1=user name*
    int ph = (int)p->trapframe->a0;
    if (ph < 0 || ph >= PROC_MAX_OBJH || p->objh_ids[ph] == (uint64_t)-1) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    result_t rname = copyinstr(p->pagetable, p->trapframe->a1, 128);
    if (!result_is_ok(rname)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    char *kname = (char *)result_unwrap(rname);
    result_t r = objfs_unlink(p->objh_ids[ph], kname);
    kfree(kname);
    p->trapframe->a0 = result_is_ok(r) ? 0 : (uint64_t)-1;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJH_RENAME: {
    // a0=parent handle, a1=user old*, a2=user new*
    int ph = (int)p->trapframe->a0;
    if (ph < 0 || ph >= PROC_MAX_OBJH || p->objh_ids[ph] == (uint64_t)-1) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    result_t ro = copyinstr(p->pagetable, p->trapframe->a1, 128);
    if (!result_is_ok(ro)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    char *ko = (char *)result_unwrap(ro);
    result_t rn = copyinstr(p->pagetable, p->trapframe->a2, 128);
    if (!result_is_ok(rn)) { kfree(ko); p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    char *kn = (char *)result_unwrap(rn);
    result_t r = objfs_rename(p->objh_ids[ph], ko, kn);
    kfree(ko); kfree(kn);
    p->trapframe->a0 = result_is_ok(r) ? 0 : (uint64_t)-1;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJ_LOOKUP_PATH: {
    // a0 = user path
    result_t rpath = copyinstr(p->pagetable, p->trapframe->a0, 256);
    if (!result_is_ok(rpath)) {
      p->trapframe->a0 = (uint64_t)-1;
      return SYSCALL_ERR_NONE;
    }
    char *kpath = (char *)result_unwrap(rpath);
    uint64_t id = 0;
    result_t r = objfs_lookup_path(kpath, &id);
    kfree(kpath);
    p->trapframe->a0 = result_is_ok(r) ? id : (uint64_t)-1;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJ_STAT: {
    // a0 = obj_id, a1 = user objfs_stat_t*
    uint64_t id = p->trapframe->a0;
    objfs_stat_t st;
    result_t r = objfs_object_stat(id, &st);
    if (!result_is_ok(r)) {
      p->trapframe->a0 = (uint64_t)-1;
      return SYSCALL_ERR_NONE;
    }
    if (!result_is_ok(copyout(p->pagetable, p->trapframe->a1, &st, sizeof(st)))) {
      p->trapframe->a0 = (uint64_t)-1;
      return SYSCALL_ERR_NONE;
    }
    p->trapframe->a0 = 0;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJ_LIST_SUBOBJECTS: {
    // a0 = dir_id, a1 = user buf, a2 = cap
    subobject_emit_ctx_t ctx = {.p = p, .dst = p->trapframe->a1, .cap = p->trapframe->a2, .wrote = 0};
    result_t r = objfs_list_subobjects(p->trapframe->a0, emit_subobject_to_user, &ctx);
    if (!result_is_ok(r))
      p->trapframe->a0 = (uint64_t)-1;
    else
      p->trapframe->a0 = ctx.wrote;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJ_READ: {
    // a0 = obj_id, a1 = user dst, a2 = offset, a3 = nbytes
    uint64_t id = p->trapframe->a0;
    uint64_t dst = p->trapframe->a1;
    uint64_t off = p->trapframe->a2;
    uint64_t n   = p->trapframe->a3;
    size_t chunk = (n > 4096) ? 4096 : (size_t)n;
    uint8_t *kbuf = (uint8_t *)kalloc(chunk);
    if (!kbuf) {
      p->trapframe->a0 = (uint64_t)-1;
      return SYSCALL_ERR_NONE;
    }
    size_t outn = 0;
    result_t r = objfs_read(id, off, kbuf, chunk, &outn);
    if (!result_is_ok(r)) {
      kfree(kbuf);
      p->trapframe->a0 = (uint64_t)-1;
      return SYSCALL_ERR_NONE;
    }
    if (!result_is_ok(copyout(p->pagetable, dst, kbuf, outn))) {
      kfree(kbuf);
      p->trapframe->a0 = (uint64_t)-1;
      return SYSCALL_ERR_NONE;
    }
    kfree(kbuf);
    p->trapframe->a0 = outn;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJ_ATTR_GET: {
    // a0=obj_id, a1=user key*, a2=user out struct*, a3=user str buf (opt), a4=cap
    result_t rkey = copyinstr(p->pagetable, p->trapframe->a1, 128);
    if (!result_is_ok(rkey)) {
      p->trapframe->a0 = (uint64_t)-1;
      return SYSCALL_ERR_NONE;
    }
    char *kkey = (char *)result_unwrap(rkey);
    uint64_t id = p->trapframe->a0;
    uint64_t outp = p->trapframe->a2;
    uint64_t strp = p->trapframe->a3;
    uint64_t cap  = p->trapframe->a4;
    // First fetch header/type
    objfs_attr_value_t av;
    result_t r = objfs_attr_get(id, kkey, &av);
    if (!result_is_ok(r)) {
      kfree(kkey);
      p->trapframe->a0 = (uint64_t)-1;
      return SYSCALL_ERR_NONE;
    }
    if (!result_is_ok(copyout(p->pagetable, outp, &av, sizeof(av)))) {
      kfree(kkey);
      p->trapframe->a0 = (uint64_t)-1;
      return SYSCALL_ERR_NONE;
    }
    if (av.type == OBJFS_ATTR_V_STR && strp && cap > 0) {
      size_t outlen = 0;
      result_t rs = objfs_attr_get_str(id, kkey, (char *)kalloc(1), 1, NULL);
      (void)rs; // ensure compiled if unused
      // copy directly into user buffer
      // We need a kernel buffer to receive string first
      char *tmp = (char *)kalloc(cap);
      if (!tmp) {
        kfree(kkey);
        p->trapframe->a0 = (uint64_t)-1;
        return SYSCALL_ERR_NONE;
      }
      result_t rg = objfs_attr_get_str(id, kkey, tmp, cap, &outlen);
      if (result_is_ok(rg)) {
        if (result_is_ok(copyout(p->pagetable, strp, tmp, outlen + 1))) {
          p->trapframe->a0 = outlen;
        } else {
          p->trapframe->a0 = (uint64_t)-1;
        }
      } else {
        p->trapframe->a0 = (uint64_t)-1;
      }
      kfree(tmp);
      kfree(kkey);
      return SYSCALL_ERR_NONE;
    }
    kfree(kkey);
    p->trapframe->a0 = 0;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJ_ATTR_LIST: {
    // a0=obj_id, a1=user buf, a2=cap
    attr_emit_ctx_t ctx = {.p = p, .dst = p->trapframe->a1, .cap = p->trapframe->a2, .wrote = 0};
    result_t r = objfs_list_attrs(p->trapframe->a0, emit_attr_to_user, &ctx);
    if (!result_is_ok(r))
      p->trapframe->a0 = (uint64_t)-1;
    else
      p->trapframe->a0 = ctx.wrote;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJ_DESC: {
    // a0=obj_id, a1=user dst (struct)
    typedef struct __attribute__((packed)) {
      uint64_t id;
      uint16_t mode;
      uint8_t  kind;
      uint8_t  flags;
      uint32_t uid;
      uint32_t gid;
      uint32_t nlink;
      uint64_t size;
      uint64_t atime, mtime, ctime;
      uint64_t target_id;
    } objfs_user_desc_t;
    uint64_t id = p->trapframe->a0;
    objfs_object_disk_t d;
    result_t r = objfs_read_descriptor(id, &d);
    if (!result_is_ok(r)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    objfs_user_desc_t ud = {
        .id = d.id, .mode = d.mode, .kind = d.kind, .flags = d.flags,
        .uid = d.uid, .gid = d.gid, .nlink = d.nlink, .size = d.size,
        .atime = d.atime, .mtime = d.mtime, .ctime = d.ctime,
        .target_id = d.target_id};
    if (!result_is_ok(copyout(p->pagetable, p->trapframe->a1, &ud, sizeof(ud)))) {
      p->trapframe->a0 = (uint64_t)-1;
      return SYSCALL_ERR_NONE;
    }
    p->trapframe->a0 = 0;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJ_CREATE: {
    // a0=parent_id, a1=user name*, a2=mode, a3=kind
    result_t rname = copyinstr(p->pagetable, p->trapframe->a1, 128);
    if (!result_is_ok(rname)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    char *kname = (char *)result_unwrap(rname);
    uint64_t parent = p->trapframe->a0;
    uint16_t mode = (uint16_t)p->trapframe->a2;
    uint8_t  kind = (uint8_t)p->trapframe->a3;
    uint64_t new_id = 0;
    result_t r = objfs_create(parent, kname, mode, kind, &new_id);
    kfree(kname);
    p->trapframe->a0 = result_is_ok(r) ? new_id : (uint64_t)-1;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJ_WRITE: {
    // a0=obj_id, a1=user src, a2=offset, a3=nbytes
    uint64_t id = p->trapframe->a0;
    uint64_t src = p->trapframe->a1;
    uint64_t off = p->trapframe->a2;
    uint64_t n   = p->trapframe->a3;
    if (n > 4096) n = 4096; // simple cap
    uint8_t *kbuf = (uint8_t *)kalloc((size_t)n);
    if (!kbuf) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    if (!result_is_ok(copyin(p->pagetable, kbuf, src, (size_t)n))) {
      kfree(kbuf);
      p->trapframe->a0 = (uint64_t)-1;
      return SYSCALL_ERR_NONE;
    }
    size_t outn = 0;
    result_t r = objfs_write_bytes(id, off, kbuf, (size_t)n, &outn);
    kfree(kbuf);
    p->trapframe->a0 = result_is_ok(r) ? outn : (uint64_t)-1;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJ_SET_ATTR: {
    // a0=obj_id, a1=user key*, a2=type, a3=user value ptr
    result_t rkey = copyinstr(p->pagetable, p->trapframe->a1, 128);
    if (!result_is_ok(rkey)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    char *kkey = (char *)result_unwrap(rkey);
    uint64_t id = p->trapframe->a0;
    uint8_t type = (uint8_t)p->trapframe->a2;
    result_t rr = RESULT_FAILURE(RESULT_INVALID);
    if (type == OBJFS_ATTR_V_STR) {
      // a4 = strlen cap, copy string
      uint64_t cap = p->trapframe->a4;
      if (cap == 0 || cap > 1024) cap = 1024;
      char *tmp = (char *)kalloc((size_t)cap);
      if (!tmp) { kfree(kkey); p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
      if (!result_is_ok(copyin(p->pagetable, tmp, p->trapframe->a3, (size_t)cap))) {
        kfree(tmp); kfree(kkey); p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE;
      }
      rr = objfs_set_attr_str(id, kkey, tmp);
      kfree(tmp);
    } else if (type == OBJFS_ATTR_V_INT) {
      int64_t v = 0;
      if (!result_is_ok(copyin(p->pagetable, &v, p->trapframe->a3, sizeof(v)))) {
        kfree(kkey); p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE;
      }
      rr = objfs_set_attr_int(id, kkey, v);
    } else if (type == OBJFS_ATTR_V_BOOL) {
      uint8_t b = 0;
      if (!result_is_ok(copyin(p->pagetable, &b, p->trapframe->a3, sizeof(b)))) {
        kfree(kkey); p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE;
      }
      rr = objfs_set_attr_bool(id, kkey, b);
    }
    kfree(kkey);
    p->trapframe->a0 = result_is_ok(rr) ? 0 : (uint64_t)-1;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJ_LINK: {
    // a0=parent_id, a1=user name*, a2=target_id
    result_t rname = copyinstr(p->pagetable, p->trapframe->a1, 128);
    if (!result_is_ok(rname)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    char *kname = (char *)result_unwrap(rname);
    result_t r = objfs_link(p->trapframe->a0, kname, p->trapframe->a2);
    kfree(kname);
    p->trapframe->a0 = result_is_ok(r) ? 0 : (uint64_t)-1;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJ_UNLINK: {
    // a0=parent_id, a1=user name*
    result_t rname = copyinstr(p->pagetable, p->trapframe->a1, 128);
    if (!result_is_ok(rname)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    char *kname = (char *)result_unwrap(rname);
    result_t r = objfs_unlink(p->trapframe->a0, kname);
    kfree(kname);
    p->trapframe->a0 = result_is_ok(r) ? 0 : (uint64_t)-1;
    return SYSCALL_ERR_NONE;
  }
  case SYSCALL_NUM_OBJ_RENAME: {
    // a0=parent_id, a1=user old*, a2=user new*
    result_t ro = copyinstr(p->pagetable, p->trapframe->a1, 128);
    if (!result_is_ok(ro)) { p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    char *ko = (char *)result_unwrap(ro);
    result_t rn = copyinstr(p->pagetable, p->trapframe->a2, 128);
    if (!result_is_ok(rn)) { kfree(ko); p->trapframe->a0 = (uint64_t)-1; return SYSCALL_ERR_NONE; }
    char *kn = (char *)result_unwrap(rn);
    result_t r = objfs_rename(p->trapframe->a0, ko, kn);
    kfree(ko); kfree(kn);
    p->trapframe->a0 = result_is_ok(r) ? 0 : (uint64_t)-1;
    return SYSCALL_ERR_NONE;
  }
  default:
    return SYSCALL_ERR_NONEXISTENT_CALLNUM;
  }
}

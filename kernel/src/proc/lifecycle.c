#include "lifecycle.h"
#include <fs/objectfs/objfs.h>
#include <include/vessel.h>
#include <limine_requests.h>
#include "lib/kalloc.h"
#include "buddy_allocator.h"
#include "memory.h"
#include "notification.h"
#include "spine.h"
#include "thread.h"
#include "process.h"
#include "process_table.h"
#include "scheduler.h"
#include "sleep.h"
#include <lib/cpu.h>
#include <lib/memory.h>
#include <lib/print.h>
#include <lib/log.h>
#include <lib/str.h>
#include <lib/usermem.h>
#include <mem_layout.h>
#include <page_table.h>

#define PROC_LIFECYCLE_DEBUG_LEVEL 0
#define USER_STACK_SIZE   (1 * 1024 * 1024ULL) /* 1 MiB user stack */
#define USER_STACK_GUARD  PAGE_SIZE            /* guard below notif region */
#define USER_STACK_TOP    (NOTIF_BUF_BASE - USER_STACK_GUARD)

extern void forkret();

static inline log_t *proc_lifecycle_log() {
  static log_t *l = NULL;
  if (!l) {
    l = g_log_create("proc", "lifecycle");
    #if PROC_LIFECYCLE_DEBUG_LEVEL >= 1
    g_log_set_level(l, LOG_LEVEL_DEBUG);
    #else
    g_log_set_level(l, LOG_LEVEL_INFO);
    #endif
  }
  return l;
}

static void teardown_user_stack(proc_t *p, uint64_t base, uint64_t top) {
  if (!p || !p->pagetable)
    return;
  for (uint64_t va = base; va < top; va += PAGE_SIZE) {
    uint64_t pa = 0;
    if (!get_physical_address(p->pagetable, va, &pa))
      continue;
    unmap_page(p->pagetable, va);
    kfree((void *)(pa + hhdm_offset));
  }
}

static g_bool setup_user_stack(proc_t *p) {
  if (!p || !p->pagetable || !p->trapframe)
    return false;

  const uint64_t stack_top = USER_STACK_TOP;
  const uint64_t stack_base = stack_top - USER_STACK_SIZE;

  if (stack_base <= p->sz) {
    LOG_ERROR(proc_lifecycle_log(),
              "stack_base (0x%{type: hex}) overlaps heap end (0x%{type: hex}) "
              "for pid=%{type: int}",
              stack_base, p->sz, p->pid);
    return false;
  }

  for (uint64_t va = stack_base; va < stack_top; va += PAGE_SIZE) {
    void *page = buddy_alloc_page();
    if (!page) {
      teardown_user_stack(p, stack_base, va);
      return false;
    }
    memset(page, 0, PAGE_SIZE);
    if (!map_page(p->pagetable, va, V2P((uint64_t)page),
                  PTE_U | PTE_V | PTE_R | PTE_W)) {
      kfree(page);
      teardown_user_stack(p, stack_base, va);
      return false;
    }
  }

  p->stack_base = stack_base;
  p->stack_top = stack_top;
  p->trapframe->sp = stack_top;
  return true;
}

g_bool killed(proc_t *p) {
  g_bool is_killed = false;
  acquire(&p->lock);
  if (p->killed) {
    is_killed = true;
  }
  release(&p->lock);
  return is_killed;
}

RESULT_TYPE(proc_t *) make_proc() {
  proc_t *p = NULL;

  for (uint8_t i = 0; i < NPROC; i++) {
    p = &processes[i];
    acquire(&p->lock);
    if (p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }

  return RESULT_FAILURE(RESULT_BUSY);

found:

  p->pid = allocate_pid();
  p->tg_leader = p;
  p->tgid = (uint32_t)p->pid;
  p->is_thread = 0;
  __atomic_store_n(&p->tg_running_cpu, -1, __ATOMIC_RELAXED);
  p->state = USED;
  p->priority = PROC_PRIORITY_NORMAL;

  struct trapframe *tf = (struct trapframe *)buddy_alloc_page();
  if (!tf) {
    release(&p->lock);
    return RESULT_FAILURE(RESULT_NOMEM);
  }
  p->trapframe = tf;

  page_table_t *pt = allocate_process_page_table(p);
  if (!pt) {
    kfree(p->trapframe);
    p->trapframe = NULL;
    release(&p->lock);
    return RESULT_FAILURE(RESULT_NOMEM);
  }

  p->pagetable = pt;
  p->heap_base = 0;
  p->stack_base = 0;
  p->stack_top = 0;

  memset(&p->context, 0, sizeof(context_t));
  p->context.ra = (uint64_t)forkret;
  p->context.sp = p->kstack + KSTACK_PAGES * PAGE_SIZE;

  // Initialize notification subsystem for this process
  notification_init_proc(p);
  // Initialize Spine for this process (token, service name)
  spine_init_proc(p);

  // Initialize FD table
  for (int i = 0; i < PROC_MAX_FD; i++) p->fd_table[i] = NULL;
  // Install stdio into unified descriptors instead of legacy FDs
  descriptor_t *d = NULL;
  int h;
  h = desc_alloc(p, &d, NULL);
  if (h >= 0 && d) { d->type = DESC_DEV_CONSOLE; d->rights = DESC_RIGHT_W; }
  h = desc_alloc(p, &d, NULL);
  if (h >= 0 && d) { d->type = DESC_DEV_UART; d->rights = DESC_RIGHT_W; }
  // Initialize unified descriptor table
  for (int i = 0; i < PROC_MAX_DESC; i++) p->desc_table[i] = NULL;
  // Initialize ObjectFS handle table
  for (int i = 0; i < PROC_MAX_OBJH; i++) {
    p->objh_ids[i] = (uint64_t)-1; /* UINT64_MAX sentinel for free */
    p->objh_flags[i] = 0;
  }

#if PROC_LIFECYCLE_DEBUG_LEVEL >= 1
  LOG_DEBUG(proc_lifecycle_log(), "alloc proc kstack = %{type: hex}",
            p->kstack);
#endif

  return RESULT_SUCCESS(p);
}

void free_process(proc_t *p) {
  if (!p)
    return;

  // Threads share their leader's address space; they must not free it.
  if (proc_is_thread(p)) {
    if (p->trapframe) {
      buddy_free_page(p->trapframe);
      p->trapframe = NULL;
    }
    p->pagetable = NULL;
    p->sz = 0;
    p->heap_base = 0;
    p->stack_base = 0;
    p->stack_top = 0;
    p->pid = 0;
    p->tg_leader = NULL;
    p->tgid = 0;
    p->is_thread = 0;
    __atomic_store_n(&p->tg_running_cpu, -1, __ATOMIC_RELAXED);
    p->parent = 0;
    p->name[0] = 0;
    p->chan = 0;
    p->killed = 0;
    p->xstate = 0;
    p->state = UNUSED;
    return;
  }

  // Reap any remaining threads in this process's thread group.
  thread_group_reap(p);

  // Clear Spine state (e.g., advertised service)
  spine_on_exit(p);

  // Close any legacy file descriptors (none after unification)

  if (p->pagetable) {
    buddy_free_page(p->pagetable);
    p->pagetable = NULL;
  }

  if (p->trapframe) {
    buddy_free_page(p->trapframe);
    p->trapframe = NULL;
  }

  p->sz = 0;
  p->heap_base = 0;
  p->stack_base = 0;
  p->stack_top = 0;
  p->pid = 0;
  p->tg_leader = NULL;
  p->tgid = 0;
  p->is_thread = 0;
  __atomic_store_n(&p->tg_running_cpu, -1, __ATOMIC_RELAXED);
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;

  p->state = UNUSED;
}

void reparent(proc_t *p) {
  for (uint64_t i = 0; i < NPROC; i++) {
    proc_t *child = &processes[i];
    if (child->parent == p) {
      child->parent = init_proc;
      wakeup(init_proc);
    }
  }
}

void exit(uint64_t status) {
  proc_t *caller = current_proc();
  proc_t *leader = proc_group(caller);

  if (leader == init_proc)
    panic("init proc exiting");

  acquire(&wait_lock);

  reparent(leader);
  wakeup(leader->parent);

  // Exit is process-wide: mark the entire thread group as ZOMBIE.
  for (uint8_t i = 0; i < NPROC; i++) {
    proc_t *q = &processes[i];
    if (q == caller) continue;
    acquire(&q->lock);
    if (q->state != UNUSED && proc_group(q) == leader) {
      q->xstate = (int)status;
      q->state = ZOMBIE;
      wakeup(q); // wake joiners
    }
    release(&q->lock);
  }

  acquire(&caller->lock);
  caller->xstate = (int)status;
  caller->state = ZOMBIE;

  release(&wait_lock);

  sched();
  panic("zombie exit");
}

uint64_t wait(uint64_t address) {
  proc_t *pp;
  g_bool has_children = false;
  uint64_t pid;
  proc_t *p = proc_group(current_proc());

#if PROC_LIFECYCLE_DEBUG_LEVEL >= 3
  LOG_DEBUG(proc_lifecycle_log(),
            "proc %{type: int} (%{type: str}) entering wait", p->pid,
            p->name);
#endif

  acquire(&wait_lock);

  for (;;) {
    has_children = 0;

    for (uint8_t i = 0; i < NPROC; i++) {
      pp = &processes[i];
      if (pp->parent != p || pp->is_thread)
        continue;
#if PROC_LIFECYCLE_DEBUG_LEVEL >= 3
      LOG_DEBUG(proc_lifecycle_log(),
                "proc %{type: int} (%{type: str}) found child proc %{type: int} (%{type: str}) in state %{type: int}",
                p->pid, p->name, pp->pid, pp->name, pp->state);
#endif

      acquire(&pp->lock);
      has_children = 1;

      if (pp->state == ZOMBIE) {
#if PROC_LIFECYCLE_DEBUG_LEVEL >= 2
        LOG_DEBUG(proc_lifecycle_log(),
                  "proc %{type: int} (%{type: str}) reaping child proc %{type: int} (%{type: str})",
                  p->pid, p->name, pp->pid, pp->name);
#endif

        pid = pp->pid;
        if (address != 0 &&
            !result_is_ok(copyout(p->pagetable, address, (void *)&pp->xstate,
                                  sizeof(pp->xstate)))) {
          release(&pp->lock);
          release(&wait_lock);
          return -1;
        }

        free_process(pp);
        release(&pp->lock);
        release(&wait_lock);
        return pid;
      }

      release(&pp->lock);
    }

    if (!has_children || killed(p)) {
#if PROC_LIFECYCLE_DEBUG_LEVEL >= 1
      if (!has_children) {
        LOG_DEBUG(proc_lifecycle_log(),
                  "proc %{type: int} (%{type: str}) has no children",
                  p->pid, p->name);
      }

      if (killed(p)) {
        LOG_DEBUG(proc_lifecycle_log(),
                  "proc %{type: int} (%{type: str}) was killed", p->pid,
                  p->name);
      }
#endif

      release(&wait_lock);
      return -1;
    }

    sleep(p, &wait_lock);
  }
}

RESULT_TYPE(void) kill(uint64_t pid) {
  proc_t *p;

  for (uint8_t i = 0; i < NPROC; i++) {
    p = &processes[i];
    acquire(&p->lock);
    if ((uint64_t)p->pid == pid) {
      p->killed = 1;
      if (p->state == SLEEPING) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return RESULT_SUCCESS(0);
    }
    release(&p->lock);
  }

  return RESULT_FAILURE(RESULT_NOT_FOUND);
}

void setkilled(proc_t *p) {
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

uint64_t fork(void) {
  uint64_t pid;

  proc_t *p = current_proc();

  result_t rnew_proc = make_proc();
  if (!result_is_ok(rnew_proc)) {
    return -1;
  }

  proc_t *new_proc = (proc_t *)result_unwrap(rnew_proc);

  if (!uvmcopy(p, new_proc)) {
    free_process(new_proc);
    return -1;
  }

  *(new_proc->trapframe) = *(p->trapframe);
  new_proc->trapframe->a0 = 0;

  pid = new_proc->pid;

  release(&new_proc->lock);

  acquire(&wait_lock);
  new_proc->parent = proc_group(p);
  release(&wait_lock);

  acquire(&new_proc->lock);
  new_proc->state = RUNNABLE;
  release(&new_proc->lock);

  return pid;
}

RESULT_TYPE(proc_t *)
proc_from_code(uint8_t code[], uint64_t size, const char *name) {
  proc_t *p = NULL;

  result_t rp = make_proc();
  if (!result_is_ok(rp)) {
    return RESULT_FAILURE(RESULT_NOMEM);
  }

  p = (proc_t *)result_unwrap(rp);

  // Allocate user memory up to size and copy code to VA=0
  uint64_t newsz = ((size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
  if (!uvmalloc(p, 0, newsz)) {
    free_process(p);
    release(&p->lock);
    return RESULT_FAILURE(RESULT_NOMEM);
  }

  // Copy program bytes into mapped region
  uint64_t remaining = size;
  uint64_t offset = 0;
  while (remaining > 0) {
    uint64_t chunk = remaining;
    // copyout copies from kernel buffer to user VA space
    if (!result_is_ok(copyout(p->pagetable, offset, code + offset, chunk))) {
      uvmdealloc(p, newsz, 0);
      free_process(p);
      release(&p->lock);
      return RESULT_FAILURE(RESULT_ERROR);
    }
    offset += chunk;
    remaining -= chunk;
  }

  p->sz = newsz;

  // Set initial trapframe for user entry
  p->trapframe->epc = 0;    // entry point at 0
  p->heap_base = newsz;
  p->stack_base = 0;
  p->stack_top = 0;
  if (!setup_user_stack(p)) {
    uvmdealloc(p, newsz, 0);
    free_process(p);
    release(&p->lock);
    return RESULT_FAILURE(RESULT_NOMEM);
  }

  if (name != NULL)
    strncopy(p->name, name, sizeof(p->name));

  p->state = RUNNABLE;
  release(&p->lock);
  return RESULT_SUCCESS(p);
}

static uint64_t align_up(uint64_t x, uint64_t a) { return (x + a - 1) & ~(a - 1); }

RESULT_TYPE(proc_t *) proc_from_vessel_path(const char *path83, const char *name) {
  if (!shared_disk_initialized || !shared_disk) {
    return RESULT_FAILURE(RESULT_ERROR);
  }

  // Read file into a temporary kernel buffer (cap slightly under 4 MiB so kalloc fits in 1024 pages)
  const uint32_t MAX_VESSEL_BYTES = (4 * 1024 * 1024) - 4096; /* keep <= 1023 pages incl. header */
  uint8_t *filebuf = (uint8_t *)kalloc(MAX_VESSEL_BYTES);
  if (!filebuf) return RESULT_FAILURE(RESULT_NOMEM);

  size_t nbytes = 0;
  // Load from ObjectFS at root ("/<name>")
  char objpath[128];
  objpath[0] = '/';
  uint64_t i = 0;
  while (path83[i] && i + 2 < sizeof(objpath)) { objpath[i+1] = path83[i]; i++; }
  objpath[i+1] = '\0';
  uint64_t file_id = 0;
  result_t rlp = objfs_lookup_path(objpath, &file_id);
  if (result_is_ok(rlp)) {
    /*
     * Support two layouts:
     *  1) Legacy: path points directly to a Vessel file object (flat .VES/.vessel)
     *  2) New:   path points to a directory-like object with attribute vessel=true
     *            whose subobject "exe" contains the actual Vessel file. The root
     *            vessel object may also have textual metadata as its own contents.
     */
    uint64_t payload_obj_id = file_id;
    g_bool has_vessel_attr = false;
    objfs_attr_value_t av;
    result_t rav = objfs_attr_get(file_id, "vessel", &av);
    if (result_is_ok(rav) && av.type == OBJFS_ATTR_V_BOOL && av.v.b) {
      has_vessel_attr = true;
    }
    // If it's a vessel object, or if it's a dir with an "exe" child, resolve to that child
    objfs_stat_t st;
    (void)objfs_object_stat(file_id, &st);
    if (has_vessel_attr || st.kind == OBJFS_OBJ_DIR) {
      // Try to find subobject named "exe"
      uint64_t cnt = 0;
      (void)objfs_subobjects_count(file_id, &cnt);
      for (uint64_t idx = 0; idx < cnt; idx++) {
        char namebuf[65];
        uint8_t kind = 0;
        uint64_t cid = 0;
        if (!result_is_ok(objfs_subobject_at(file_id, idx, namebuf, sizeof(namebuf), &kind, &cid)))
          continue;
        if (namebuf[0] == 'e' && namebuf[1] == 'x' && namebuf[2] == 'e' && namebuf[3] == '\0') {
          payload_obj_id = cid;
          break;
        }
      }
    }
    // Read payload from resolved object id
    size_t outn = 0;
    result_t rr = objfs_read(payload_obj_id, 0, filebuf, MAX_VESSEL_BYTES, &outn);
    if (result_is_ok(rr)) {
      nbytes = outn;
    } else {
      kfree(filebuf);
      LOG_WARN(proc_lifecycle_log(),
               "failed to read vessel file from path %{type: str}: %{result: err}",
               path83, rr);
      return RESULT_FAILURE(RESULT_ERROR);
    }
  } else {
    kfree(filebuf);
    LOG_WARN(proc_lifecycle_log(),
             "vessel file not found at path %{type: str}: %{result: err}",
             path83, rlp);
    return RESULT_FAILURE(RESULT_NOT_FOUND);
  }
  if (nbytes < sizeof(vessel_hdr_t)) {
    kfree(filebuf);
    LOG_WARN(proc_lifecycle_log(),
             "vessel file at path %{type: str} too small (%{type: int} bytes)",
             path83, nbytes);
    return RESULT_FAILURE(RESULT_ERROR);
  }

  vessel_hdr_t *hdr = (vessel_hdr_t *)filebuf;
  if (hdr->magic != VESSEL_MAGIC_U64) {
    kfree(filebuf);
    LOG_WARN(proc_lifecycle_log(),
             "vessel file at path %{type: str} has invalid magic (%{type: hex})",
             path83, hdr->magic);
    return RESULT_FAILURE(RESULT_ERROR);
  }

  uint32_t cmds_size = hdr->commands_size;
  if (sizeof(vessel_hdr_t) + cmds_size > nbytes) {
    kfree(filebuf);
    LOG_WARN(proc_lifecycle_log(),
             "vessel file at path %{type: str} has invalid commands size (%{type: int})",
             path83, cmds_size);
    return RESULT_FAILURE(RESULT_ERROR);
  }

  uint8_t *cmdp = filebuf + sizeof(vessel_hdr_t);
  uint8_t *cmd_end = cmdp + cmds_size;

  uint64_t entry = 0;
  g_bool have_entry = false;
  uint64_t max_vend = 0;

  // First pass: validate and compute max end
  while (cmdp + sizeof(vessel_cmd_t) <= cmd_end) {
    vessel_cmd_t *c = (vessel_cmd_t *)cmdp;
    if (c->size < sizeof(vessel_cmd_t)) { kfree(filebuf); return RESULT_FAILURE(RESULT_ERROR); }
    if (cmdp + c->size > cmd_end) { kfree(filebuf); return RESULT_FAILURE(RESULT_ERROR); }
    if (c->type == VESSEL_CMD_SEGMENT) {
      if (c->size < sizeof(vessel_cmd_segment_t)) { kfree(filebuf); return RESULT_FAILURE(RESULT_ERROR); }
      vessel_cmd_segment_t *s = (vessel_cmd_segment_t *)c;
      uint64_t vend = s->vaddr + s->mem_size;
      if (vend > max_vend) max_vend = vend;
      // Basic bounds for file payload
      if (s->file_offset + s->file_size > nbytes) { kfree(filebuf); return RESULT_FAILURE(RESULT_ERROR); }
    } else if (c->type == VESSEL_CMD_ENTRY_POINT) {
      if (c->size < sizeof(vessel_cmd_entry_point_t)) { kfree(filebuf); return RESULT_FAILURE(RESULT_ERROR); }
      vessel_cmd_entry_point_t *e = (vessel_cmd_entry_point_t *)c;
      entry = e->entry;
      have_entry = true;
    }
    cmdp += c->size;
  }

  if (!have_entry) {
    // Require an explicit ENTRY_POINT command
    kfree(filebuf);
    LOG_WARN(proc_lifecycle_log(),
             "vessel file at path %{type: str} missing entry point command",
             path83);
    return RESULT_FAILURE(RESULT_ERROR);
  }

  // Create process and map segments
  result_t rp = make_proc();
  if (!result_is_ok(rp)) { kfree(filebuf); return RESULT_FAILURE(RESULT_NOMEM); }
  proc_t *p = (proc_t *)result_unwrap(rp);

  // Second pass: map segments and copy data
  cmdp = filebuf + sizeof(vessel_hdr_t);
  while (cmdp + sizeof(vessel_cmd_t) <= cmd_end) {
    vessel_cmd_t *c = (vessel_cmd_t *)cmdp;
    if (c->type == VESSEL_CMD_SEGMENT) {
      vessel_cmd_segment_t *s = (vessel_cmd_segment_t *)c;
      uint64_t vstart = s->vaddr;
      uint64_t msize = s->mem_size;
      uint64_t foff  = s->file_offset;
      uint64_t fsize = s->file_size;
      uint64_t flags = PTE_U | PTE_V | (s->flags & VESSEL_SEG_R ? PTE_R : 0) |
                       (s->flags & VESSEL_SEG_W ? PTE_W : 0) |
                       (s->flags & VESSEL_SEG_X ? PTE_X : 0);

      uint64_t pend = vstart + msize;
      uint64_t cur = vstart & ~(PAGE_SIZE - 1);
      while (cur < pend) {
        void *page = buddy_alloc_page();
        if (!page) { free_process(p); kfree(filebuf); return RESULT_FAILURE(RESULT_NOMEM); }
        memset(page, 0, PAGE_SIZE);
        uint64_t pa = (uint64_t)page - hhdm_offset;
        if (!map_page(p->pagetable, cur, pa, flags)) {
          buddy_free_page(page);
          LOG_WARN(proc_lifecycle_log(),
                   "failed to map vessel segment page at va %{type: hex} for pid %{type: int}",
                   cur, p->pid);
          free_process(p); kfree(filebuf); return RESULT_FAILURE(RESULT_ERROR);
        }
        cur += PAGE_SIZE;
      }

      // Copy file payload into mapped memory at vaddr
      if (fsize > 0) {
        // print destination address of payload
        LOG_DEBUG(proc_lifecycle_log(), "destination address of payload: %{type: hex}\n", vstart);

        if (!result_is_ok(copyout(p->pagetable, vstart, filebuf + foff, fsize))) {
          free_process(p); kfree(filebuf); return RESULT_FAILURE(RESULT_ERROR);
        }
      }
    }
    cmdp += c->size;
  }

  p->heap_base = align_up(max_vend, PAGE_SIZE);
  p->sz = p->heap_base;
  if (!setup_user_stack(p)) {
    free_process(p);
    release(&p->lock);
    kfree(filebuf);
    return RESULT_FAILURE(RESULT_NOMEM);
  }

  p->trapframe->epc = entry;

  if (name) strncopy(p->name, name, sizeof(p->name));
  p->state = RUNNABLE;
  release(&p->lock);

  kfree(filebuf);
  return RESULT_SUCCESS(p);
}

static uint64_t align_down(uint64_t x, uint64_t a) { return x & ~(a - 1); }

static g_bool push_one_string(proc_t *p, const char *s, uint64_t *sp_io, uint64_t *out_va) {
  const uint64_t MAX_STR = 256;
  char kbuf[MAX_STR];
  uint64_t len = 0;
  while (s && s[len] && len + 1 < MAX_STR) len++;
  for (uint64_t i = 0; i < len; i++) kbuf[i] = s[i];
  kbuf[len] = '\0';
  uint64_t need = len + 1;
  uint64_t sp = *sp_io - need;
  if (!result_is_ok(copyout(p->pagetable, sp, kbuf, need))) return false;
  *sp_io = sp;
  *out_va = sp;
  return true;
}

static g_bool push_argv_on_user_stack(proc_t *p,
                                      const char *name,
                                      uint64_t argc,
                                      const char * const *argv,
                                      uint64_t *out_argc,
                                      uint64_t *out_argv_va,
                                      uint64_t *out_new_sp) {
  if (!p || !p->pagetable || !p->trapframe) return false;
  // We will place strings first, then build argv array above them, keeping alignment.
  // Reserve some headroom.
  uint64_t sp = p->trapframe->sp;
  const uint64_t PTR_SIZE = sizeof(uint64_t);
  // Build argv0 from name or empty
  const char *argv0 = name ? name : "";
  // Count total args including argv0
  uint64_t n = argc + 1;
  // Place strings from top down
  uint64_t str_addrs_cap = n;
  uint64_t *str_addrs = (uint64_t *)kalloc(sizeof(uint64_t) * str_addrs_cap);
  if (!str_addrs) return false;
  uint64_t idx = 0;
  uint64_t va = 0;
  if (!push_one_string(p, argv0, &sp, &va)) { kfree(str_addrs); return false; }
  str_addrs[idx++] = va;
  for (uint64_t i = 0; i < argc; i++) {
    const char *as = argv[i] ? argv[i] : "";
    if (!push_one_string(p, as, &sp, &va)) { kfree(str_addrs); return false; }
    str_addrs[idx++] = va;
  }
  // Align stack for argv array
  sp = align_down(sp, PTR_SIZE);
  // Add NULL terminator for argv array
  sp -= PTR_SIZE;
  uint64_t zero = 0;
  if (!result_is_ok(copyout(p->pagetable, sp, &zero, PTR_SIZE))) { kfree(str_addrs); return false; }
  // Push pointers in reverse so argv[0] ends up lowest address after NULL
  for (int64_t i = (int64_t)n - 1; i >= 0; i--) {
    sp -= PTR_SIZE;
    uint64_t ptr = str_addrs[i];
    if (!result_is_ok(copyout(p->pagetable, sp, &ptr, PTR_SIZE))) { kfree(str_addrs); return false; }
  }
  kfree(str_addrs);
  *out_argc = n;
  *out_argv_va = sp;
  *out_new_sp = sp;
  return true;
}

RESULT_TYPE(proc_t *) proc_from_vessel_path_args(const char *path83,
                                                 const char *name,
                                                 uint64_t argc,
                                                 const char * const *argv) {
  if (!shared_disk_initialized || !shared_disk) {
    return RESULT_FAILURE(RESULT_ERROR);
  }
  const uint32_t MAX_VESSEL_BYTES = (4 * 1024 * 1024) - 4096;
  uint8_t *filebuf = (uint8_t *)kalloc(MAX_VESSEL_BYTES);
  if (!filebuf) return RESULT_FAILURE(RESULT_NOMEM);
  size_t nbytes = 0;
  char objpath[128];
  objpath[0] = '/';
  uint64_t i = 0;
  while (path83[i] && i + 2 < sizeof(objpath)) { objpath[i+1] = path83[i]; i++; }
  objpath[i+1] = '\0';
  uint64_t file_id = 0;
  result_t rlp = objfs_lookup_path(objpath, &file_id);
  if (result_is_ok(rlp)) {
    uint64_t payload_obj_id = file_id;
    g_bool has_vessel_attr = false;
    objfs_attr_value_t av;
    result_t rav = objfs_attr_get(file_id, "vessel", &av);
    if (result_is_ok(rav) && av.type == OBJFS_ATTR_V_BOOL && av.v.b) {
      has_vessel_attr = true;
    }
    objfs_stat_t st;
    (void)objfs_object_stat(file_id, &st);
    if (has_vessel_attr || st.kind == OBJFS_OBJ_DIR) {
      uint64_t cnt = 0;
      (void)objfs_subobjects_count(file_id, &cnt);
      for (uint64_t idx = 0; idx < cnt; idx++) {
        char namebuf[65];
        uint8_t kind = 0;
        uint64_t cid = 0;
        if (!result_is_ok(objfs_subobject_at(file_id, idx, namebuf, sizeof(namebuf), &kind, &cid)))
          continue;
        if (namebuf[0] == 'e' && namebuf[1] == 'x' && namebuf[2] == 'e' && namebuf[3] == '\0') {
          payload_obj_id = cid;
          break;
        }
      }
    }
    size_t outn = 0;
    result_t rr = objfs_read(payload_obj_id, 0, filebuf, MAX_VESSEL_BYTES, &outn);
    if (result_is_ok(rr)) {
      nbytes = outn;
    } else {
      kfree(filebuf);
      return RESULT_FAILURE(RESULT_ERROR);
    }
  } else {
    kfree(filebuf);
    return RESULT_FAILURE(RESULT_NOT_FOUND);
  }
  if (nbytes < sizeof(vessel_hdr_t)) {
    kfree(filebuf);
    return RESULT_FAILURE(RESULT_ERROR);
  }
  vessel_hdr_t *hdr = (vessel_hdr_t *)filebuf;
  if (hdr->magic != VESSEL_MAGIC_U64) {
    kfree(filebuf);
    return RESULT_FAILURE(RESULT_ERROR);
  }
  uint32_t cmds_size = hdr->commands_size;
  if (sizeof(vessel_hdr_t) + cmds_size > nbytes) {
    kfree(filebuf);
    return RESULT_FAILURE(RESULT_ERROR);
  }
  uint8_t *cmdp = filebuf + sizeof(vessel_hdr_t);
  uint8_t *cmd_end = cmdp + cmds_size;
  uint64_t entry = 0;
  g_bool have_entry = false;
  uint64_t max_vend = 0;
  while (cmdp + sizeof(vessel_cmd_t) <= cmd_end) {
    vessel_cmd_t *c = (vessel_cmd_t *)cmdp;
    if (c->size < sizeof(vessel_cmd_t)) { kfree(filebuf); return RESULT_FAILURE(RESULT_ERROR); }
    if (cmdp + c->size > cmd_end) { kfree(filebuf); return RESULT_FAILURE(RESULT_ERROR); }
    if (c->type == VESSEL_CMD_SEGMENT) {
      if (c->size < sizeof(vessel_cmd_segment_t)) { kfree(filebuf); return RESULT_FAILURE(RESULT_ERROR); }
      vessel_cmd_segment_t *s = (vessel_cmd_segment_t *)c;
      uint64_t vend = s->vaddr + s->mem_size;
      if (vend > max_vend) max_vend = vend;
      if (s->file_offset + s->file_size > nbytes) { kfree(filebuf); return RESULT_FAILURE(RESULT_ERROR); }
    } else if (c->type == VESSEL_CMD_ENTRY_POINT) {
      if (c->size < sizeof(vessel_cmd_entry_point_t)) { kfree(filebuf); return RESULT_FAILURE(RESULT_ERROR); }
      vessel_cmd_entry_point_t *e = (vessel_cmd_entry_point_t *)c;
      entry = e->entry;
      have_entry = true;
    }
    cmdp += c->size;
  }
  if (!have_entry) { kfree(filebuf); return RESULT_FAILURE(RESULT_ERROR); }
  result_t rp = make_proc();
  if (!result_is_ok(rp)) { kfree(filebuf); return RESULT_FAILURE(RESULT_NOMEM); }
  proc_t *p = (proc_t *)result_unwrap(rp);
  cmdp = filebuf + sizeof(vessel_hdr_t);
  while (cmdp + sizeof(vessel_cmd_t) <= cmd_end) {
    vessel_cmd_t *c = (vessel_cmd_t *)cmdp;
    if (c->type == VESSEL_CMD_SEGMENT) {
      vessel_cmd_segment_t *s = (vessel_cmd_segment_t *)c;
      uint64_t vstart = s->vaddr;
      uint64_t msize = s->mem_size;
      uint64_t foff  = s->file_offset;
      uint64_t fsize = s->file_size;
      uint64_t flags = PTE_U | PTE_V | (s->flags & VESSEL_SEG_R ? PTE_R : 0) |
                       (s->flags & VESSEL_SEG_W ? PTE_W : 0) |
                       (s->flags & VESSEL_SEG_X ? PTE_X : 0);
      uint64_t pend = vstart + msize;
      uint64_t cur = vstart & ~(PAGE_SIZE - 1);
      while (cur < pend) {
        void *page = buddy_alloc_page();
        if (!page) { free_process(p); kfree(filebuf); return RESULT_FAILURE(RESULT_NOMEM); }
        memset(page, 0, PAGE_SIZE);
        uint64_t pa = (uint64_t)page - hhdm_offset;
        if (!map_page(p->pagetable, cur, pa, flags)) {
          buddy_free_page(page);
          free_process(p); kfree(filebuf); return RESULT_FAILURE(RESULT_ERROR);
        }
        cur += PAGE_SIZE;
      }
      if (fsize > 0) {
        if (!result_is_ok(copyout(p->pagetable, vstart, filebuf + foff, fsize))) {
          free_process(p); kfree(filebuf); return RESULT_FAILURE(RESULT_ERROR);
        }
      }
    }
    cmdp += c->size;
  }
  p->heap_base = align_up(max_vend, PAGE_SIZE);
  p->sz = p->heap_base;
  if (!setup_user_stack(p)) {
    free_process(p);
    release(&p->lock);
    kfree(filebuf);
    return RESULT_FAILURE(RESULT_NOMEM);
  }
  p->trapframe->epc = entry;
  // Install argv now, before making RUNNABLE
  uint64_t final_argc = 0, argv_va = 0, new_sp = 0;
  if (!push_argv_on_user_stack(p, name ? name : path83, argc, argv, &final_argc, &argv_va, &new_sp)) {
    free_process(p);
    release(&p->lock);
    kfree(filebuf);
    return RESULT_FAILURE(RESULT_ERROR);
  }
  p->trapframe->a0 = final_argc;
  p->trapframe->a1 = argv_va;
  p->trapframe->sp = new_sp;
  if (name) strncopy(p->name, name, sizeof(p->name));
  p->state = RUNNABLE;
  release(&p->lock);
  kfree(filebuf);
  return RESULT_SUCCESS(p);
}

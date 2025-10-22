#include "syscall.h"
#include "lib/log.h"
#include "lib/macros.h"
#include "lib/print.h"
#include "lib/result.h"
#include "lib/usermem.h"
#include "lifecycle.h"
#include "proc/notification.h"

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
                "syscall dispatched from [%{type: int}]: %{type: string}",
                p->pid, se.desc);
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
  default:
    return SYSCALL_ERR_NONEXISTENT_CALLNUM;
    break;
  }
};

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
    notif_ctx_restore_to_trapframe(p);
    return SYSCALL_ERR_NONE;
  } else {
    return SYSCALL_ERR_NONEXISTENT_CALLNUM;
  }
};

syscall_err_t syscall_handle_work(proc_t *p, syscall_num_t num) {
  if (num == SYSCALL_NUM_PRINT_INT) {
    printf("%{type: int}\n", PRINT_FLAG_BOTH, (int64_t)p->trapframe->a0);
    return SYSCALL_ERR_NONE;
  } else if (num == SYSCALL_NUM_PRINT_STR) {
    result_t rstr = copyinstr(p->pagetable, p->trapframe->a0, 256);
    if (!result_is_ok(rstr)) {
      LOG_WARN(syscall_log(),
               "sys_print_str: failed to copy string from user memory");
      return SYSCALL_ERR_INVALID_ARG;
    }
    char *ustr = (char *)result_unwrap(rstr);

    printf("%{type: str}", PRINT_FLAG_BOTH, ustr);
    return SYSCALL_ERR_NONE;
  } else {
    return SYSCALL_ERR_NONEXISTENT_CALLNUM;
  }

  return SYSCALL_ERR_NONE;
};

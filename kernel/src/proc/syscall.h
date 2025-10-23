#pragma once

#include "proc/process.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum syscall_num {
  SYSCALL_NUM_EXIT = 0x02,

  SYSCALL_NUM_PRINT_INT = 0x10,
  SYSCALL_NUM_PRINT_STR = 0x11,

  SYSCALL_NUM_NOTIF_REGISTER = 0x90,
  SYSCALL_NUM_NOTIF_UNREGISTER = 0x91,
  SYSCALL_NUM_NOTIF_DONE = 0x100,

  /* Filesystem */
  SYSCALL_NUM_OPEN = 0x200,
  SYSCALL_NUM_READ = 0x201,
  SYSCALL_NUM_CLOSE = 0x202,
  SYSCALL_NUM_STAT = 0x203,
  SYSCALL_NUM_GETDENTS = 0x204,

  /* Named forks */
  SYSCALL_NUM_FORK_OPEN = 0x210,
  SYSCALL_NUM_FORK_LIST = 0x211,
  SYSCALL_NUM_FORK_STAT = 0x212,

  /* Write syscall for console out and future files */
  SYSCALL_NUM_WRITE = 0x205,
} syscall_num_t;

typedef enum syscall_err {
  SYSCALL_ERR_NONE = 0,
  SYSCALL_ERR_INVALID_ARG = -1,
  SYSCALL_ERR_NOT_FOUND = -2,
  SYSCALL_ERR_NO_MEM = -3,
  SYSCALL_ERR_UNENTITLED = -4,

  SYSCALL_ERR_NONEXISTENT_CALLNUM = -100,
} syscall_err_t;

typedef struct syscall_entry {
  const char *desc;
  uint64_t call_number;
  syscall_err_t (*handler)(proc_t *, syscall_num_t);
} syscall_entry_t;

// runs the proper handlers from the table
syscall_err_t syscall_dispatch(proc_t *p, syscall_num_t num);

// process lifecycle calls (e.g. exit, spawn, etc.)
syscall_err_t syscall_handle_lifecycle(proc_t *p, syscall_num_t num);

// notification registration and more
syscall_err_t syscall_handle_notification(proc_t *p, syscall_num_t num);

// syscall relating to work in progress functionality or debugging.
syscall_err_t syscall_handle_work(proc_t *p, syscall_num_t num);

// filesystem syscalls
syscall_err_t syscall_handle_fs(proc_t *p, syscall_num_t num);

// syscall table
static const syscall_entry_t syscall_table[] = {
    {"exit()", SYSCALL_NUM_EXIT, syscall_handle_lifecycle},

    {"print_int()", SYSCALL_NUM_PRINT_INT, syscall_handle_work},
    {"print_str()", SYSCALL_NUM_PRINT_STR, syscall_handle_work},

    {"notification.register()", SYSCALL_NUM_NOTIF_REGISTER,
     syscall_handle_notification},
    {"notification.unregister()", SYSCALL_NUM_NOTIF_UNREGISTER,
     syscall_handle_notification},
    {"notification.done()", SYSCALL_NUM_NOTIF_DONE,
     syscall_handle_notification},

    {"fs.open()", SYSCALL_NUM_OPEN, syscall_handle_fs},
    {"fs.read()", SYSCALL_NUM_READ, syscall_handle_fs},
    {"fs.write()", SYSCALL_NUM_WRITE, syscall_handle_fs},
    {"fs.close()", SYSCALL_NUM_CLOSE, syscall_handle_fs},
    {"fs.stat()", SYSCALL_NUM_STAT, syscall_handle_fs},
    {"fs.getdents()", SYSCALL_NUM_GETDENTS, syscall_handle_fs},
    {"fork.open()", SYSCALL_NUM_FORK_OPEN, syscall_handle_fs},
    {"fork.list()", SYSCALL_NUM_FORK_LIST, syscall_handle_fs},
    {"fork.stat()", SYSCALL_NUM_FORK_STAT, syscall_handle_fs},
};

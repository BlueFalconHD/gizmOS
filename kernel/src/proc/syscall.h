#pragma once

#include "proc/process.h"
#include <stdbool.h>
#include <stdint.h>
#include "../../../include/syscall_numbers.h"

typedef enum syscall_num {
  SYSCALL_NUM_EXIT = SYSNO_EXIT,
  SYSCALL_NUM_SPAWN = SYSNO_SPAWN,
  SYSCALL_NUM_WAIT = SYSNO_WAIT,
  SYSCALL_NUM_SPAWN2 = SYSNO_SPAWN2,

  SYSCALL_NUM_PRINT_INT = SYSNO_PRINT_INT,
  SYSCALL_NUM_PRINT_STR = SYSNO_PRINT_STR,
  SYSCALL_NUM_SBRK      = SYSNO_SBRK,

  SYSCALL_NUM_NOTIF_REGISTER = SYSNO_NOTIF_REGISTER,
  SYSCALL_NUM_NOTIF_UNREGISTER = SYSNO_NOTIF_UNREGISTER,
  SYSCALL_NUM_NOTIF_DONE = SYSNO_NOTIF_DONE,

  /* Spine IPC */
  SYSCALL_NUM_SPINE_MSG = SYSNO_SPINE_MSG,
  SYSCALL_NUM_SPINE_SERVICE_ADVERTISE = SYSNO_SPINE_SERVICE_ADVERTISE,
  SYSCALL_NUM_SPINE_SERVICE_LOOKUP = SYSNO_SPINE_SERVICE_LOOKUP,
  SYSCALL_NUM_SPINE_GET_SEAL = SYSNO_SPINE_GET_SEAL,

  /* ObjectFS (read-only) */
  SYSCALL_NUM_OBJ_LOOKUP_PATH = SYSNO_OBJ_LOOKUP_PATH,
  SYSCALL_NUM_OBJ_STAT = SYSNO_OBJ_STAT,
  SYSCALL_NUM_OBJ_LIST_SUBOBJECTS = SYSNO_OBJ_LIST_SUBOBJECTS,
  SYSCALL_NUM_OBJ_READ = SYSNO_OBJ_READ,
  SYSCALL_NUM_OBJ_ATTR_GET = SYSNO_OBJ_ATTR_GET,
  SYSCALL_NUM_OBJ_ATTR_LIST = SYSNO_OBJ_ATTR_LIST,
  SYSCALL_NUM_OBJ_DESC = SYSNO_OBJ_DESC,

  /* ObjectFS (mutation - phase 2) */
  SYSCALL_NUM_OBJ_CREATE = SYSNO_OBJ_CREATE,
  SYSCALL_NUM_OBJ_WRITE = SYSNO_OBJ_WRITE,
  SYSCALL_NUM_OBJ_SET_ATTR = SYSNO_OBJ_SET_ATTR,
  SYSCALL_NUM_OBJ_LINK = SYSNO_OBJ_LINK,
  SYSCALL_NUM_OBJ_UNLINK = SYSNO_OBJ_UNLINK,
  SYSCALL_NUM_OBJ_RENAME = SYSNO_OBJ_RENAME,

  /* Object handle API (requested) */
  SYSCALL_NUM_OBJH_ID_AT = SYSNO_OBJH_ID_AT,          // obj_id_at(path)
  SYSCALL_NUM_OBJH_OPEN = SYSNO_OBJH_OPEN,           // obj_open(obj_id, flags)
  SYSCALL_NUM_OBJH_CLOSE = SYSNO_OBJH_CLOSE,          // obj_close(handle)
  SYSCALL_NUM_OBJH_HAS_SUBS = SYSNO_OBJH_HAS_SUBS,       // obj_has_subobjects(handle)
  SYSCALL_NUM_OBJH_SUBS_COUNT = SYSNO_OBJH_SUBS_COUNT,     // obj_get_subobject_count(handle)
  SYSCALL_NUM_OBJH_SUB_AT = SYSNO_OBJH_SUB_AT,         // obj_get_subobject_at(handle, index) -> id
  SYSCALL_NUM_OBJH_OPEN_AT = SYSNO_OBJH_OPEN_AT,        // objh_open_at(path, flags) -> handle
  SYSCALL_NUM_OBJH_STAT = SYSNO_OBJH_STAT,           // objh_stat(handle, out)
  SYSCALL_NUM_OBJH_LIST_SUBOBJECTS = SYSNO_OBJH_LIST_SUBOBJECTS,// objh_list_subobjects(handle, buf, cap)
  SYSCALL_NUM_OBJH_READ = SYSNO_OBJH_READ,           // objh_read(handle, dst, off, n)
  SYSCALL_NUM_OBJH_WRITE = SYSNO_OBJH_WRITE,          // objh_write(handle, src, off, n)
  SYSCALL_NUM_OBJH_SEEK = SYSNO_OBJH_SEEK,   // provisional: objh_seek(handle, off, whence)
  SYSCALL_NUM_OBJH_ATTR_GET = SYSNO_OBJH_ATTR_GET,       // objh_attr_get(handle, key, out, strbuf, cap)
  SYSCALL_NUM_OBJH_ATTR_LIST = SYSNO_OBJH_ATTR_LIST,      // objh_attr_list(handle, buf, cap)
  SYSCALL_NUM_OBJH_DESC = SYSNO_OBJH_DESC,           // objh_desc(handle, out)
  SYSCALL_NUM_OBJH_CREATE = SYSNO_OBJH_CREATE,         // objh_create(parent_handle, name, mode, kind) -> handle
  SYSCALL_NUM_OBJH_SET_ATTR = SYSNO_OBJH_SET_ATTR,       // objh_set_attr(handle, key, type, value)
  SYSCALL_NUM_OBJH_LINK = SYSNO_OBJH_LINK,           // objh_link(parent_handle, name, target_handle)
  SYSCALL_NUM_OBJH_UNLINK = SYSNO_OBJH_UNLINK,         // objh_unlink(parent_handle, name)
  SYSCALL_NUM_OBJH_RENAME = SYSNO_OBJH_RENAME,         // objh_rename(parent_handle, old, new)
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

// spine IPC syscalls
syscall_err_t syscall_handle_spine(proc_t *p, syscall_num_t num);

// syscall relating to work in progress functionality or debugging.
syscall_err_t syscall_handle_work(proc_t *p, syscall_num_t num);

// memory management syscalls
syscall_err_t syscall_handle_memory(proc_t *p, syscall_num_t num);

// filesystem syscalls
syscall_err_t syscall_handle_fs(proc_t *p, syscall_num_t num);

// syscall table
static const syscall_entry_t syscall_table[] = {
    {"exit()", SYSCALL_NUM_EXIT, syscall_handle_lifecycle},
    {"spawn()", SYSCALL_NUM_SPAWN, syscall_handle_lifecycle},
    {"wait()", SYSCALL_NUM_WAIT, syscall_handle_lifecycle},
    {"spawn2()", SYSCALL_NUM_SPAWN2, syscall_handle_lifecycle},

    {"print_int()", SYSCALL_NUM_PRINT_INT, syscall_handle_work},
    {"print_str()", SYSCALL_NUM_PRINT_STR, syscall_handle_work},
    {"sbrk()", SYSCALL_NUM_SBRK, syscall_handle_memory},

    {"notification.register()", SYSCALL_NUM_NOTIF_REGISTER,
     syscall_handle_notification},
    {"notification.unregister()", SYSCALL_NUM_NOTIF_UNREGISTER,
     syscall_handle_notification},
    {"notification.done()", SYSCALL_NUM_NOTIF_DONE,
     syscall_handle_notification},

    {"spine.msg()", SYSCALL_NUM_SPINE_MSG, syscall_handle_spine},
    {"spine.service_advertise()", SYSCALL_NUM_SPINE_SERVICE_ADVERTISE, syscall_handle_spine},
    {"spine.service_lookup()", SYSCALL_NUM_SPINE_SERVICE_LOOKUP, syscall_handle_spine},
    {"spine.get_seal()", SYSCALL_NUM_SPINE_GET_SEAL, syscall_handle_spine},

    {"obj.lookup_path()", SYSCALL_NUM_OBJ_LOOKUP_PATH, syscall_handle_fs},
    {"obj.stat()", SYSCALL_NUM_OBJ_STAT, syscall_handle_fs},
    {"obj.list_subobjects()", SYSCALL_NUM_OBJ_LIST_SUBOBJECTS, syscall_handle_fs},
    {"obj.read()", SYSCALL_NUM_OBJ_READ, syscall_handle_fs},
    {"obj.attr_get()", SYSCALL_NUM_OBJ_ATTR_GET, syscall_handle_fs},
    {"obj.attr_list()", SYSCALL_NUM_OBJ_ATTR_LIST, syscall_handle_fs},
    {"obj.desc()", SYSCALL_NUM_OBJ_DESC, syscall_handle_fs},
    {"obj.create()", SYSCALL_NUM_OBJ_CREATE, syscall_handle_fs},
    {"obj.write()", SYSCALL_NUM_OBJ_WRITE, syscall_handle_fs},
    {"obj.attr_set()", SYSCALL_NUM_OBJ_SET_ATTR, syscall_handle_fs},
    {"obj.link()", SYSCALL_NUM_OBJ_LINK, syscall_handle_fs},
    {"obj.unlink()", SYSCALL_NUM_OBJ_UNLINK, syscall_handle_fs},
    {"obj.rename()", SYSCALL_NUM_OBJ_RENAME, syscall_handle_fs},

    {"objh.id_at()", SYSCALL_NUM_OBJH_ID_AT, syscall_handle_fs},
    {"objh.open()", SYSCALL_NUM_OBJH_OPEN, syscall_handle_fs},
    {"objh.close()", SYSCALL_NUM_OBJH_CLOSE, syscall_handle_fs},
    {"objh.has_subs()", SYSCALL_NUM_OBJH_HAS_SUBS, syscall_handle_fs},
    {"objh.subs_count()", SYSCALL_NUM_OBJH_SUBS_COUNT, syscall_handle_fs},
    {"objh.sub_at()", SYSCALL_NUM_OBJH_SUB_AT, syscall_handle_fs},
    {"objh.open_at()", SYSCALL_NUM_OBJH_OPEN_AT, syscall_handle_fs},
    {"objh.stat()", SYSCALL_NUM_OBJH_STAT, syscall_handle_fs},
    {"objh.list_subobjects()", SYSCALL_NUM_OBJH_LIST_SUBOBJECTS, syscall_handle_fs},
    {"objh.read()", SYSCALL_NUM_OBJH_READ, syscall_handle_fs},
    {"objh.write()", SYSCALL_NUM_OBJH_WRITE, syscall_handle_fs},
    {"objh.attr_get()", SYSCALL_NUM_OBJH_ATTR_GET, syscall_handle_fs},
    {"objh.attr_list()", SYSCALL_NUM_OBJH_ATTR_LIST, syscall_handle_fs},
    {"objh.desc()", SYSCALL_NUM_OBJH_DESC, syscall_handle_fs},
    {"objh.create()", SYSCALL_NUM_OBJH_CREATE, syscall_handle_fs},
    {"objh.attr_set()", SYSCALL_NUM_OBJH_SET_ATTR, syscall_handle_fs},
    {"objh.link()", SYSCALL_NUM_OBJH_LINK, syscall_handle_fs},
    {"objh.unlink()", SYSCALL_NUM_OBJH_UNLINK, syscall_handle_fs},
    {"objh.rename()", SYSCALL_NUM_OBJH_RENAME, syscall_handle_fs},
};

#pragma once

#include "proc/process.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum syscall_num {
  SYSCALL_NUM_EXIT = 0x02,
  SYSCALL_NUM_SPAWN = 0x03,
  SYSCALL_NUM_WAIT = 0x04,

  SYSCALL_NUM_PRINT_INT = 0x10,
  SYSCALL_NUM_PRINT_STR = 0x11,

  SYSCALL_NUM_NOTIF_REGISTER = 0x90,
  SYSCALL_NUM_NOTIF_UNREGISTER = 0x91,
  SYSCALL_NUM_NOTIF_DONE = 0x100,

  /* Spine IPC */
  SYSCALL_NUM_SPINE_MSG_SEND = 0x180,
  SYSCALL_NUM_SPINE_SERVICE_ADVERTISE = 0x181,
  SYSCALL_NUM_SPINE_SERVICE_LOOKUP = 0x182,
  SYSCALL_NUM_SPINE_GET_SEAL = 0x183,

  /* ObjectFS (read-only) */
  SYSCALL_NUM_OBJ_LOOKUP_PATH = 0x220,
  SYSCALL_NUM_OBJ_STAT = 0x221,
  SYSCALL_NUM_OBJ_LIST_SUBOBJECTS = 0x222,
  SYSCALL_NUM_OBJ_READ = 0x223,
  SYSCALL_NUM_OBJ_ATTR_GET = 0x224,
  SYSCALL_NUM_OBJ_ATTR_LIST = 0x225,
  SYSCALL_NUM_OBJ_DESC = 0x226,

  /* ObjectFS (mutation - phase 2) */
  SYSCALL_NUM_OBJ_CREATE = 0x230,
  SYSCALL_NUM_OBJ_WRITE = 0x231,
  SYSCALL_NUM_OBJ_SET_ATTR = 0x232,
  SYSCALL_NUM_OBJ_LINK = 0x233,
  SYSCALL_NUM_OBJ_UNLINK = 0x234,
  SYSCALL_NUM_OBJ_RENAME = 0x235,

  /* Object handle API (requested) */
  SYSCALL_NUM_OBJH_ID_AT = 0x240,          // obj_id_at(path)
  SYSCALL_NUM_OBJH_OPEN = 0x241,           // obj_open(obj_id, flags)
  SYSCALL_NUM_OBJH_CLOSE = 0x242,          // obj_close(handle)
  SYSCALL_NUM_OBJH_HAS_SUBS = 0x243,       // obj_has_subobjects(handle)
  SYSCALL_NUM_OBJH_SUBS_COUNT = 0x244,     // obj_get_subobject_count(handle)
  SYSCALL_NUM_OBJH_SUB_AT = 0x245,         // obj_get_subobject_at(handle, index) -> id
  SYSCALL_NUM_OBJH_OPEN_AT = 0x246,        // objh_open_at(path, flags) -> handle
  SYSCALL_NUM_OBJH_STAT = 0x247,           // objh_stat(handle, out)
  SYSCALL_NUM_OBJH_LIST_SUBOBJECTS = 0x248,// objh_list_subobjects(handle, buf, cap)
  SYSCALL_NUM_OBJH_READ = 0x249,           // objh_read(handle, dst, off, n)
  SYSCALL_NUM_OBJH_WRITE = 0x24A,          // objh_write(handle, src, off, n)
  SYSCALL_NUM_OBJH_SEEK = 0x24A + 0x100,   // provisional: objh_seek(handle, off, whence)
  SYSCALL_NUM_OBJH_ATTR_GET = 0x24B,       // objh_attr_get(handle, key, out, strbuf, cap)
  SYSCALL_NUM_OBJH_ATTR_LIST = 0x24C,      // objh_attr_list(handle, buf, cap)
  SYSCALL_NUM_OBJH_DESC = 0x24D,           // objh_desc(handle, out)
  SYSCALL_NUM_OBJH_CREATE = 0x24E,         // objh_create(parent_handle, name, mode, kind) -> handle
  SYSCALL_NUM_OBJH_SET_ATTR = 0x24F,       // objh_set_attr(handle, key, type, value)
  SYSCALL_NUM_OBJH_LINK = 0x250,           // objh_link(parent_handle, name, target_handle)
  SYSCALL_NUM_OBJH_UNLINK = 0x251,         // objh_unlink(parent_handle, name)
  SYSCALL_NUM_OBJH_RENAME = 0x252,         // objh_rename(parent_handle, old, new)
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

// filesystem syscalls
syscall_err_t syscall_handle_fs(proc_t *p, syscall_num_t num);

// syscall table
static const syscall_entry_t syscall_table[] = {
    {"exit()", SYSCALL_NUM_EXIT, syscall_handle_lifecycle},
    {"spawn()", SYSCALL_NUM_SPAWN, syscall_handle_lifecycle},
    {"wait()", SYSCALL_NUM_WAIT, syscall_handle_lifecycle},

    {"print_int()", SYSCALL_NUM_PRINT_INT, syscall_handle_work},
    {"print_str()", SYSCALL_NUM_PRINT_STR, syscall_handle_work},

    {"notification.register()", SYSCALL_NUM_NOTIF_REGISTER,
     syscall_handle_notification},
    {"notification.unregister()", SYSCALL_NUM_NOTIF_UNREGISTER,
     syscall_handle_notification},
    {"notification.done()", SYSCALL_NUM_NOTIF_DONE,
     syscall_handle_notification},

    {"spine.msg_send()", SYSCALL_NUM_SPINE_MSG_SEND, syscall_handle_spine},
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

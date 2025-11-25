// Minimal ObjectFS handle wrappers for userland
#pragma once

#include "../sys/syscall.h"

// Opaque handle type for ObjectFS objects (directories, files, devices).
typedef long obj_handle_t;

// Open an object by path and return a handle (or < 0 on error).
static inline obj_handle_t obj_open(const char *path, unsigned long flags) {
  return sys_objh_open_at(path, flags);
}

// Close a previously opened handle.
static inline long obj_close(obj_handle_t h) {
  return sys_objh_close(h);
}

// List subobjects of a directory handle into a buffer of sys_objdirent_t.
// Returns number of bytes written, or < 0 on error.
static inline long obj_getdents(obj_handle_t h, sys_objdirent_t *buf, long cap) {
  return sys_objh_list_subobjects(h, buf, cap);
}

// Positional read from an object handle.
// Returns number of bytes read, or < 0 on error.
static inline long obj_pread(obj_handle_t h, void *dst, unsigned long offset, long n) {
  return sys_objh_read(h, dst, offset, n);
}



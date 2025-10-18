#include "dyn_array.h"
#include "lib/debug.h"
#include "lib/print.h"
#include <lib/kalloc.h>
#include <lib/memory.h>
#include <lib/panic.h>
/* page allocator no longer used here */

#define KB 1024
#define MB (1024 * KB)

static void *alloc_block(g_usize elem_sz, g_usize cap) {
  return kalloc(elem_sz * cap);
}

RESULT_TYPE(dyn_array_t *)
make_dyn_array(g_usize elem_size, g_usize initial_capacity) {
  dyn_array_t *a = (dyn_array_t *)kalloc(sizeof(dyn_array_t));
  if (!a) {
    dbg("kalloc(...) == NULL");
    return RESULT_FAILURE(RESULT_NOMEM);
  }

  if (!dyn_array_init(a, elem_size, initial_capacity)) {
    kfree(a);
    dbg("dyn_array_init(...) == false");
    return RESULT_FAILURE(RESULT_ERROR);
  }
  return RESULT_SUCCESS(a);
}

g_bool dyn_array_init(dyn_array_t *a, g_usize elem_size,
                      g_usize initial_capacity) {
  if (!a || elem_size == 0) {
    if (!a)
      dbg("a == NULL");
    else
      dbg("elem_size == 0");
    return false;
  }

  a->data = alloc_block(elem_size, initial_capacity);
  if (!a->data) {
    dbg("alloc_block(...) == NULL");
    return false;
  }

  a->elem_size = elem_size;
  a->cap = initial_capacity;
  a->len = 0;
  a->is_initialized = true;
  return true;
}

void dyn_array_free(dyn_array_t *a) {
  if (!a || !a->is_initialized) {
    if (!a)
      dbg("a == NULL");
    else
      dbg("!a->is_initialized");
    return;
  }

  kfree(a->data);
  a->data = NULL;
  a->cap = a->len = 0;
  a->is_initialized = false;
}

static g_bool grow(dyn_array_t *a) {
  g_usize new_cap = a->cap * 2;
  if (new_cap == 0)
    new_cap = 1;

  void *new_block = alloc_block(a->elem_size, new_cap);
  if (!new_block) {
    dbg("alloc_block(...) == NULL");
    return false;
  }

  memcpy(new_block, a->data, a->len * a->elem_size);
  kfree(a->data);
  a->data = new_block;
  a->cap = new_cap;
  return true;
}

g_bool dyn_array_push(dyn_array_t *a, const void *elem) {
  if (!a || !a->is_initialized || !elem) {
    if (!a)
      dbg("a == NULL");
    else if (!a->is_initialized)
      dbg("!a->is_initialized");
    else
      dbg("elem == NULL");
    return false;
  }

  if (a->len == a->cap) {
    if (!grow(a)) {
      dbg("grow(...) == false");
      return false;
    }
  }

  uint8_t *dst = ((uint8_t *)a->data) + (a->len * a->elem_size);
  memcpy(dst, elem, a->elem_size);
  a->len++;
  return true;
}

void *dyn_array_get(dyn_array_t *a, g_usize index) {
  if (!a || !a->is_initialized || index >= a->len) {
    if (!a)
      dbg("a == NULL");
    else if (!a->is_initialized)
      dbg("!a->is_initialized");
    else
      dbg("index >= a->len");
    return NULL;
  }
  return ((uint8_t *)a->data) + (index * a->elem_size);
}

void dyn_array_clear(dyn_array_t *a) {
  if (!a || !a->is_initialized) {
    if (!a)
      dbg("a == NULL");
    else
      dbg("!a->is_initialized");
    return;
  }
  a->len = 0;
}

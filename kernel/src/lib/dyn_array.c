#include "dyn_array.h"
#include "lib/print.h"
#include <lib/kalloc.h>
#include <lib/memory.h>
#include <lib/panic.h>
#include <physical_alloc.h>

#define KB 1024
#define MB (1024 * KB)

static void *alloc_block(g_usize elem_sz, g_usize cap) {
  return kalloc(elem_sz * cap);
}

RESULT_TYPE(dyn_array_t *)
make_dyn_array(g_usize elem_size, g_usize initial_capacity) {
  dyn_array_t *a = (dyn_array_t *)kalloc(sizeof(dyn_array_t));
  if (!a)
    return RESULT_FAILURE(RESULT_NOMEM);

  if (!dyn_array_init(a, elem_size, initial_capacity)) {
    free_page(a);
    return RESULT_FAILURE(RESULT_ERROR);
  }
  return RESULT_SUCCESS(a);
}

g_bool dyn_array_init(dyn_array_t *a, g_usize elem_size,
                      g_usize initial_capacity) {
  if (!a || elem_size == 0)
    return false;

  a->data = alloc_block(elem_size, initial_capacity);
  if (!a->data)
    return false;

  a->elem_size = elem_size;
  a->cap = initial_capacity;
  a->len = 0;
  a->is_initialized = true;
  return true;
}

void dyn_array_free(dyn_array_t *a) {
  if (!a || !a->is_initialized)
    return;

  kfree(a->data);
  a->data = NULL;
  a->cap = a->len = 0;
  a->is_initialized = false;
}

static g_bool grow(dyn_array_t *a) {
  printf("dyn_array: grow len:%{type: int} cap:%{type: int}\n", PRINT_FLAG_BOTH,
         a->len, a->cap);

  g_usize new_cap = a->cap * 2;
  if (new_cap == 0)
    new_cap = 1;

  if (new_cap > KB / a->elem_size) {
    return false;
  }

  void *new_block = alloc_block(a->elem_size, new_cap);
  if (!new_block)
    return false;

  memcpy(new_block, a->data, a->len * a->elem_size);
  kfree(a->data);
  a->data = new_block;
  a->cap = new_cap;
  return true;
}

g_bool dyn_array_push(dyn_array_t *a, const void *elem) {
  if (!a || !a->is_initialized || !elem)
    return false;

  if (a->len == a->cap) {
    if (!grow(a))
      return false;
  }

  uint8_t *dst = ((uint8_t *)a->data) + (a->len * a->elem_size);
  memcpy(dst, elem, a->elem_size);
  a->len++;
  return true;
}

void *dyn_array_get(dyn_array_t *a, g_usize index) {
  if (!a || !a->is_initialized || index >= a->len)
    return NULL;
  return ((uint8_t *)a->data) + (index * a->elem_size);
}

void dyn_array_clear(dyn_array_t *a) {
  if (!a || !a->is_initialized)
    return;
  a->len = 0;
}

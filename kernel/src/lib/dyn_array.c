#include "dyn_array.h"
#include <lib/memory.h>
#include <lib/panic.h>
#include <physical_alloc.h>

/* internal helper – returns number of elements that fit into one page      */
static inline g_usize max_elems_in_page(g_usize elem_sz) {
  return (PAGE_SIZE / elem_sz);
}

/* allocate backing storage fitting at least `cap` elements (one page max)  */
static void *alloc_block(g_usize elem_sz, g_usize cap) {
  if (cap == 0 || cap > max_elems_in_page(elem_sz))
    return NULL;
  return alloc_page();
}

/* -------------------------------------------------------------------------- */
/*  Constructors / life‑cycle                                                 */
/* -------------------------------------------------------------------------- */
RESULT_TYPE(dyn_array_t *)
make_dyn_array(g_usize elem_size, g_usize initial_capacity) {
  dyn_array_t *a = (dyn_array_t *)alloc_page();
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

  free_page(a->data);
  a->data = NULL;
  a->cap = a->len = 0;
  a->is_initialized = false;
  /* caller may free `a` itself if it was heap‑allocated */
}

static g_bool grow(dyn_array_t *a) {
  g_usize new_cap = a->cap * 2;
  if (new_cap == 0) /* was empty → minimum 1                   */
    new_cap = 1;

  if (new_cap > max_elems_in_page(a->elem_size))
    return false; /* cannot exceed one 4‑KiB page            */

  void *new_block = alloc_block(a->elem_size, new_cap);
  if (!new_block)
    return false;

  memcpy(new_block, a->data, a->len * a->elem_size);
  free_page(a->data);
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

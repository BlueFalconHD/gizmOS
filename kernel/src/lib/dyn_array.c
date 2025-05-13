#include "dyn_array.h"
#include "lib/print.h"
#include <lib/memory.h>
#include <lib/panic.h>
#include <physical_alloc.h>

#define MAX_PAGES_PER_BLOCK 1024 /* arbitrary safety limit              */

static inline g_usize elems_per_page(g_usize elem_sz) {
  return (PAGE_SIZE / elem_sz);
}

static inline g_usize pages_for(g_usize elem_sz, g_usize cap) {
  uint64_t bytes = elem_sz * cap;
  return (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
}

/* Allocate `pages` *contiguous* pages; return base or NULL on failure */
static void *alloc_contiguous_pages(g_usize pages) {
  if (pages == 0 || pages > MAX_PAGES_PER_BLOCK)
    return NULL;

  void *base = NULL;
  void *prev = NULL;

  for (g_usize i = 0; i < pages; i++) {
    void *page = alloc_page();
    if (!page) { /* out of memory          */
      goto fail;
    }

    if (i == 0) {
      base = page;
    } else {
      /* expect the allocator to hand out descending contiguous pages */
      if ((uint64_t)page + PAGE_SIZE != (uint64_t)prev) {
        goto fail; /* not contiguous         */
      }
    }
    prev = page;
  }
  return base;

fail: /* free any pages taken   */
  if (base) {
    for (void *p = base; p < (uint8_t *)base + pages * PAGE_SIZE;
         p = (uint8_t *)p + PAGE_SIZE)
      free_page(p);
  }
  return NULL;
}

/* Allocate backing storage for at least `cap` elements                  */
static void *alloc_block(g_usize elem_sz, g_usize cap) {
  if (cap == 0)
    return NULL;

  g_usize pages = pages_for(elem_sz, cap);
  return alloc_contiguous_pages(pages);
}

static void free_block(void *base, g_usize elem_sz, g_usize cap) {
  if (!base)
    return;
  g_usize pages = pages_for(elem_sz, cap);
  for (g_usize i = 0; i < pages; i++)
    free_page((uint8_t *)base + i * PAGE_SIZE);
}

/* ----------------------------------------------------- constructors ---- */
RESULT_TYPE(dyn_array_t *)
make_dyn_array(g_usize elem_sz, g_usize initial_cap) {
  dyn_array_t *a = (dyn_array_t *)alloc_page();
  if (!a)
    return RESULT_FAILURE(RESULT_NOMEM);

  if (!dyn_array_init(a, elem_sz, initial_cap)) {
    free_page(a);
    return RESULT_FAILURE(RESULT_ERROR);
  }
  return RESULT_SUCCESS(a);
}

g_bool dyn_array_init(dyn_array_t *a, g_usize elem_sz, g_usize initial_cap) {
  if (!a || elem_sz == 0)
    return false;

  a->data = alloc_block(elem_sz, initial_cap);
  if (!a->data)
    return false;

  a->elem_size = elem_sz;
  a->cap = initial_cap;
  a->len = 0;
  a->is_initialized = true;
  return true;
}

void dyn_array_free(dyn_array_t *a) {
  if (!a || !a->is_initialized)
    return;

  free_block(a->data, a->elem_size, a->cap);
  a->data = NULL;
  a->cap = a->len = 0;
  a->is_initialized = false;
}

/* ------------------------------------------------------------- grow ---- */
static g_bool grow(dyn_array_t *a) {
  printf("dyn_array: grow len:%{type: int} cap:%{type: int}\n", PRINT_FLAG_BOTH,
         a->len, a->cap);

  g_usize new_cap = (a->cap == 0) ? 1 : a->cap * 2;

  void *new_block = alloc_block(a->elem_size, new_cap);
  if (!new_block)
    return false;

  memcpy(new_block, a->data, a->len * a->elem_size);
  free_block(a->data, a->elem_size, a->cap);

  a->data = new_block;
  a->cap = new_cap;
  return true;
}

/* ------------------------------------------------------------ API ----- */
g_bool dyn_array_push(dyn_array_t *a, const void *elem) {
  if (!a || !a->is_initialized || !elem)
    return false;

  if (a->len == a->cap && !grow(a))
    return false;

  uint8_t *dst = (uint8_t *)a->data + a->len * a->elem_size;
  memcpy(dst, elem, a->elem_size);
  a->len++;
  return true;
}

void *dyn_array_get(dyn_array_t *a, g_usize index) {
  if (!a || !a->is_initialized || index >= a->len)
    return NULL;
  return (uint8_t *)a->data + index * a->elem_size;
}

void dyn_array_clear(dyn_array_t *a) {
  if (a && a->is_initialized)
    a->len = 0;
}

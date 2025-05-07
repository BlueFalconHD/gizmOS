#pragma once
/*
 * Very small “vector” implementation backed by the kernel page allocator.
 * It is intentionally simple:
 *   • Elements are kept in one contiguous block obtained via alloc_page().
 *   • Capacity grows geometrically (×2) until the backing area would no
 *     longer fit into one 4‑KiB page – after that `dyn_array_push()` fails.
 *   • The API is generic (void *) but strongly typed at call‑site through
 *     the usual C macro trick.
 *
 * This is *not* meant to be a fully‑featured STL clone – only what is
 * required inside the kernel itself.
 *
 * Example:
 *     DYN_ARRAY_TYPE(uint32_t) numbers;
 *     dyn_array_init(&numbers, sizeof(uint32_t), 8);
 *     uint32_t v = 42;
 *     dyn_array_push(&numbers, &v);
 *     uint32_t first = *DYN_ARRAY_AT(&numbers, uint32_t, 0);
 */

#include <lib/result.h>
#include <lib/types.h>
#include <lib/macros.h>

typedef struct {
  void    *data;         /* backing storage (nullptr until init)      */
  g_usize  len;          /* number of *valid* elements                */
  g_usize  cap;          /* total capacity (elements, not bytes)      */
  g_usize  elem_size;    /* sizeof(T)                                 */
  g_bool   is_initialized;
} dyn_array_t;

RESULT_TYPE(dyn_array_t *) make_dyn_array(g_usize elem_size,
                                          g_usize initial_capacity);
g_bool  dyn_array_init (dyn_array_t *a,
                        g_usize      elem_size,
                        g_usize      initial_capacity);
void    dyn_array_free (dyn_array_t *a); /* releases backing page */

g_bool  dyn_array_push (dyn_array_t *a, const void *elem);
void   *dyn_array_get  (dyn_array_t *a, g_usize index); /* pointer to element */
void    dyn_array_clear(dyn_array_t *a);                 /* len -> 0          */

G_INLINE g_usize dyn_array_len(dyn_array_t *a) { return a ? a->len : 0; }

#define DYN_ARRAY_TYPE(T)        dyn_array_##T##_t

#define DYN_ARRAY_DECLARE(T)                                                 \
  typedef struct {                                                           \
    dyn_array_t inner;                                                       \
  } DYN_ARRAY_TYPE(T);                                                       \
  G_INLINE DYN_ARRAY_TYPE(T) dyn_array_##T##_make(g_usize initial_cap) {     \
    result_t r = make_dyn_array(sizeof(T), initial_cap);                     \
    return (DYN_ARRAY_TYPE(T)){ .inner = *(dyn_array_t *)result_unwrap(r) };  \
  }

#define DYN_ARRAY_PUSH(arr_ptr, value_ptr) \
        dyn_array_push(&(arr_ptr)->inner, (value_ptr))

#define DYN_ARRAY_AT(arr_ptr, T, idx) \
        ((T *)dyn_array_get(&(arr_ptr)->inner, (idx)))

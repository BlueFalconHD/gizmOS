#include <lib/memory.h>
#include <lib/result.h>
#include <limine_requests.h>
#include <page_table.h>

#define PAGE_OFFSET(addr) ((addr) & (PAGE_SIZE - 1))
#define PAGE_REMAIN(addr) (PAGE_SIZE - PAGE_OFFSET(addr))

static inline uint64_t min_u64(uint64_t a, uint64_t b) {
  return (a < b) ? a : b;
}

/*
 * Copy `len` bytes from kernel buffer `src` into user-space virtual address
 * `dstva` that is translated using `pagetable`.
 */
RESULT_TYPE(void)
copyout(page_table_t *pagetable, uint64_t dstva, void *src, uint64_t len) {
  uint8_t *kbuf = (uint8_t *)src;

  while (len > 0) {
    uint64_t pa;
    if (!get_physical_address(pagetable, dstva, &pa)) {
      return RESULT_FAILURE(RESULT_ERROR); /* unmapped user page */
    }

    uint64_t n = min_u64(len, PAGE_REMAIN(dstva));
    uint8_t *dst_kva = (uint8_t *)(pa + hhdm_offset);

    memcpy(dst_kva, kbuf, n);

    len -= n;
    dstva += n;
    kbuf += n;
  }

  return RESULT_SUCCESS(0);
}

/*
 * Copy `len` bytes from user-space virtual address `srcva` into kernel buffer
 * `dst` using `pagetable` for translation.
 */
RESULT_TYPE(void)
copyin(page_table_t *pagetable, void *dst, uint64_t srcva, uint64_t len) {
  uint8_t *kbuf = (uint8_t *)dst;

  while (len > 0) {
    uint64_t pa;
    if (!get_physical_address(pagetable, srcva, &pa)) {
      return RESULT_FAILURE(RESULT_ERROR); /* unmapped user page */
    }

    uint64_t n = min_u64(len, PAGE_REMAIN(srcva));
    uint8_t *src_kva = (uint8_t *)(pa + hhdm_offset);

    memcpy(kbuf, src_kva, n);

    len -= n;
    srcva += n;
    kbuf += n;
  }

  return RESULT_SUCCESS(0);
}

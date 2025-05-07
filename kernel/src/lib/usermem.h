#pragma once

#include <lib/result.h>
#include <stdint.h>
#include <page_table.h>

/*
 * Copy `len` bytes from the kernel buffer `src` into the user-space virtual
 * address `dstva`, translating through the given `pagetable`.
 *
 * Returns RESULT_OK on success or RESULT_ERROR if any portion of the destination
 * range is not mapped user memory.
 */
RESULT_TYPE(void) copyout(page_table_t *pagetable,
                          uint64_t      dstva,
                          void         *src,
                          uint64_t      len);

/*
 * Copy `len` bytes from the user-space virtual address `srcva` into the kernel
 * buffer `dst`, translating through the given `pagetable`.
 *
 * Returns RESULT_OK on success or RESULT_ERROR if any portion of the source
 * range is not mapped user memory.
 */
RESULT_TYPE(void) copyin(page_table_t *pagetable,
                         void         *dst,
                         uint64_t      srcva,
                         uint64_t      len);
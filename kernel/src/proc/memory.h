#pragma once

#include "process.h"
#include <lib/types.h>

page_table_t *allocate_process_page_table(proc_t *p);
g_bool proc_grow(proc_t *p, uint64_t bytes);
g_bool proc_shrink(proc_t *p, uint64_t bytes);
RESULT_TYPE(void) proc_resize(int n);
g_bool uvmalloc(proc_t *p, uint64_t oldsz, uint64_t newsz);
g_bool uvmdealloc(proc_t *p, uint64_t oldsz, uint64_t newsz);
g_bool uvmcopy(page_table_t *src, page_table_t *dst, uint64_t sz);



#include "memory.h"
#include "process.h"
#include <lib/cpu.h>
#include <lib/memory.h>
#include <mem_layout.h>
#include <page_table.h>
#include <physical_alloc.h>

extern char trampoline[];
extern uint64_t hhdm_offset;

/* local helpers formerly in proc.c */
#define PGROUNDUP(sz) (((sz) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PGROUNDDOWN(sz) ((sz) & ~(PAGE_SIZE - 1))

g_bool uvmdealloc(proc_t *p, uint64_t oldsz, uint64_t newsz); /* fwd */

g_bool uvmalloc(proc_t *p, uint64_t oldsz, uint64_t newsz) {
  if (newsz < oldsz)
    return true;

  oldsz = PGROUNDUP(oldsz);
  for (uint64_t a = oldsz; a < newsz; a += PAGE_SIZE) {
    void *mem = alloc_page();
    if (!mem)
      return false;
    memset(mem, 0, PAGE_SIZE);
    if (!map_page(p->pagetable, a, V2P((uint64_t)mem),
                  PTE_R | PTE_W | PTE_X | PTE_U | PTE_V)) {
      free_page(mem);
      uvmdealloc(p, a, oldsz);
      return false;
    }
  }
  p->sz = newsz;
  return true;
}

g_bool uvmdealloc(proc_t *p, uint64_t oldsz, uint64_t newsz) {
  if (newsz >= oldsz)
    return true;

  for (uint64_t a = PGROUNDUP(newsz); a < oldsz; a += PAGE_SIZE) {
    uint64_t pa = 0;
    if (!get_physical_address(p->pagetable, a, &pa))
      continue;
    if (!unmap_page(p->pagetable, a))
      return false;
    free_page((void *)(pa + hhdm_offset));
  }
  p->sz = newsz;
  return true;
}

g_bool uvmcopy(page_table_t *src, page_table_t *dst, uint64_t sz) {
  for (uint64_t a = 0; a < sz; a += PAGE_SIZE) {
    uint64_t pa = 0;
    if (!get_physical_address(src, a, &pa))
      return false;

    void *mem = alloc_page();
    if (!mem)
      return false;
    memcpy(mem, (void *)(pa + hhdm_offset), PAGE_SIZE);

    if (!map_page(dst, a, V2P((uint64_t)mem),
                  PTE_R | PTE_W | PTE_X | PTE_U | PTE_V)) {
      free_page(mem);
      return false;
    }
  }
  return true;
}

page_table_t *allocate_process_page_table(proc_t *p) {
  page_table_t *pt = alloc_page();
  if (!pt) {
    return NULL;
  }

  memset(pt, 0, PAGE_SIZE);

  if (!map_page(pt, TRAMPOLINE, V2P((uint64_t)trampoline),
                PTE_R | PTE_W | PTE_X | PTE_V)) {
    free_page(pt);
    return NULL;
  }

  if (!map_page(pt, TRAPFRAME, V2P((uint64_t)p->trapframe),
                PTE_R | PTE_W | PTE_X | PTE_V)) {
    free_page(pt);
    return NULL;
  }

  return pt;
}

g_bool proc_grow(proc_t *p, uint64_t bytes) {
  if (!p || bytes == 0)
    return false;
  uint64_t oldsz = p->sz;
  uint64_t newsz = oldsz + bytes;
  newsz = PGROUNDUP(newsz);
  return uvmalloc(p, oldsz, newsz);
}

g_bool proc_shrink(proc_t *p, uint64_t bytes) {
  if (!p || bytes == 0)
    return false;
  uint64_t oldsz = p->sz;
  uint64_t newsz = (bytes >= oldsz) ? 0 : oldsz - bytes;
  newsz = PGROUNDDOWN(newsz);
  return uvmdealloc(p, oldsz, newsz);
}

RESULT_TYPE(void) proc_resize(int n) {
  uint64_t sz;
  proc_t *p = current_proc();

  sz = p->sz;
  if (n > 0) {
    if (proc_grow(p, n) == false) {
      return RESULT_FAILURE(RESULT_ERROR);
    }
  } else if (n < 0) {
    if (proc_shrink(p, -n) == false) {
      return RESULT_FAILURE(RESULT_ERROR);
    }
  } else {
    return RESULT_FAILURE(RESULT_ERROR);
  }
  p->sz = sz;
  return RESULT_SUCCESS(0);
}

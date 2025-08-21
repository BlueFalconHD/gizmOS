#include "earlyinit.h"

#include "buddy_allocator.h"
#include "lib/macros.h"
#include "limine_requests.h"
#include "mem_layout.h"
#include "page_table.h"
#include "platform/interrupts.h"

#define EARLY_TEXT __attribute__((section(".text.early"), used, noinline))
#define EARLY_RODATA __attribute__((section(".rodata.early"), used))

extern void trap_vector();
extern char trap_stack_top;

extern char kstart[];
extern char kend[];
extern char trampoline[];

G_INLINE void enable_interrupts() {
  PS_enable_interrupts();
  PS_enable_all_interrupt_types();
}

G_INLINE void disable_interrupts() {
  PS_disable_interrupts();
  PS_disable_all_interrupts_types();
}

G_INLINE void init_trap_vector(void) {
  uintptr_t base = ((uintptr_t)&trap_vector) & ~0x3UL;
  PS_set_trap_vector(base);
  asm volatile("csrw sscratch, %0" ::"r"(&trap_stack_top));
}

EARLY_TEXT early_init_status early_init() {
  // HHDM offset is stored here among other boot-critical values, this MUST
  // happen first
  limine_requests_init();

  // Set trap vector up
  init_trap_vector();

  // Allocator is used for memory map stuff.
  buddy_allocator_init(memory_map_entries, memory_map_entry_count);

  page_table_t *root_page_table = create_page_table();
  if (!root_page_table) {
    return EARLY_INIT_FAIL_CREATE_TABLE;
  }

  bool success = map_range(
      root_page_table, executable_virtual_base, executable_physical_base,
      (uint64_t)kend - (uint64_t)kstart, PTE_R | PTE_W | PTE_X | PTE_V);

  if (!success) {
    panic("Failed to set up kernel mapping");
  }

  success = map_range(root_page_table, hhdm_offset + 0xc0000000, 0xc0000000,
                      0x100000000, PTE_R | PTE_W | PTE_X | PTE_V);

  if (!success) {
    return EARLY_INIT_FAIL_KERNEL_MAP;
  }

  uint64_t phys_lo = RAM_START; /* 0x8000_0000                       */
  uint64_t phys_hi = 0;         /* will become last byte of RAM      */
  for (uint64_t i = 0; i < memory_map_entry_count; i++) {
    struct limine_memmap_entry *e = memory_map_entries[i];
    if (e->type != LIMINE_MEMMAP_RESERVED) {
      uint64_t end = e->base + e->length;
      if (end > phys_hi)
        phys_hi = end;
    }
  }
  uint64_t ram_bytes = phys_hi - phys_lo;

  success =
      map_range(root_page_table, hhdm_offset + phys_lo, /* virtual start */
                phys_lo,                                /* physical start */
                ram_bytes,                              /* length in bytes */
                PTE_R | PTE_W | PTE_X | PTE_V);

  if (!success) {
    return EARLY_INIT_FAIL_FULL_RAM_MAP;
  }

  activate_page_table(root_page_table);
  shared_page_table = root_page_table;

  return EARLY_INIT_SUCCESS;
}

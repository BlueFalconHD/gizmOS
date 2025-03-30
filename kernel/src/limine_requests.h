#pragma once

#include "lib/result.h"
#include <limine.h>

extern uint64_t hhdm_offset;

extern struct limine_memmap_entry **memory_map_entries;
extern uint64_t memory_map_entry_count;

extern uint64_t executable_physical_base;
extern uint64_t executable_virtual_base;
extern uint64_t executable_size;

extern volatile struct limine_framebuffer_request limine_req_framebuffer;
extern volatile struct limine_boot_time_request limine_req_boot_time;
extern volatile struct limine_hhdm_request limine_req_hhdm_offset;
extern volatile struct limine_executable_address_request
    limine_req_executable_address;
extern volatile struct limine_executable_file_request
    limine_req_executable_file;
extern volatile struct limine_memmap_request limine_req_memory_map;
extern volatile struct limine_paging_mode_request limine_req_paging_mode;

result_t limine_requests_init();

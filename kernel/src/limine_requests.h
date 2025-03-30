// #pragma once

// #include "lib/result.h"
// #include <limine.h>

// __attribute__((
//     used, section(".limine_requests"))) static volatile
//     LIMINE_BASE_REVISION(3);

// __attribute__((used,
//                section(".limine_requests_"
//                        "start"))) static volatile
//                        LIMINE_REQUESTS_START_MARKER;

// __attribute__((
//     used,
//     section(
//         ".limine_requests_end"))) static volatile LIMINE_REQUESTS_END_MARKER;

// __attribute__((
//     used,
//     section(
//         ".limine_requests"))) static volatile struct
//         limine_framebuffer_request
//     limine_req_framebuffer = {.id = LIMINE_FRAMEBUFFER_REQUEST, .revision =
//     0};

// __attribute__((
//     used,
//     section(
//         ".limine_requests"))) static volatile struct limine_boot_time_request
//     limine_req_boot_time = {.id = LIMINE_BOOT_TIME_REQUEST, .revision = 0};

// __attribute__((
//     used,
//     section(".limine_requests"))) static volatile struct limine_hhdm_request
//     limine_req_hhdm_offset = {.id = LIMINE_HHDM_REQUEST, .revision = 0};

// __attribute__((used, section(".limine_requests"))) static volatile struct
//     limine_executable_address_request limine_req_executable_address = {
//         .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST, .revision = 0};

// __attribute__((used, section(".limine_requests"))) static volatile struct
//     limine_executable_file_request limine_req_executable_file = {
//         .id = LIMINE_EXECUTABLE_FILE_REQUEST, .revision = 0};

// __attribute__((
//     used,
//     section(".limine_requests"))) static volatile struct
//     limine_memmap_request limine_req_memory_map = {.id =
//     LIMINE_MEMMAP_REQUEST, .revision = 0};

// __attribute__((
//     used,
//     section(
//         ".limine_requests"))) static volatile struct
//         limine_paging_mode_request
//     limine_req_paging_mode = {.id = LIMINE_PAGING_MODE_REQUEST,
//                               .revision = 0,
//                               .mode = LIMINE_PAGING_MODE_RISCV_SV39};

// result_t limine_requests_init();

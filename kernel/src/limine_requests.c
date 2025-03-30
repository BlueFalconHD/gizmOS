#include "limine_requests.h"
#include <lib/panic.h>
#include <limine.h>

__attribute__((used,
               section(".limine_requests"))) volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests_"
                             "start"))) volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((
    used, section(".limine_requests_end"))) volatile LIMINE_REQUESTS_END_MARKER;

__attribute__((
    used,
    section(".limine_requests"))) volatile struct limine_framebuffer_request
    limine_req_framebuffer = {.id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

__attribute__((
    used, section(".limine_requests"))) volatile struct limine_boot_time_request
    limine_req_boot_time = {.id = LIMINE_BOOT_TIME_REQUEST, .revision = 0};

__attribute__((used,
               section(".limine_requests"))) volatile struct limine_hhdm_request
    limine_req_hhdm_offset = {.id = LIMINE_HHDM_REQUEST, .revision = 0};

__attribute__((
    used,
    section(
        ".limine_requests"))) volatile struct limine_executable_address_request
    limine_req_executable_address = {.id = LIMINE_EXECUTABLE_ADDRESS_REQUEST,
                                     .revision = 0};

__attribute__((
    used,
    section(".limine_requests"))) volatile struct limine_executable_file_request
    limine_req_executable_file = {.id = LIMINE_EXECUTABLE_FILE_REQUEST,
                                  .revision = 0};

__attribute__((
    used, section(".limine_requests"))) volatile struct limine_memmap_request
    limine_req_memory_map = {.id = LIMINE_MEMMAP_REQUEST, .revision = 0};

__attribute__((
    used,
    section(".limine_requests"))) volatile struct limine_paging_mode_request
    limine_req_paging_mode = {.id = LIMINE_PAGING_MODE_REQUEST,
                              .revision = 0,
                              .mode = LIMINE_PAGING_MODE_RISCV_SV39};

uint64_t hhdm_offset;

struct limine_memmap_entry **memory_map_entries;
uint64_t memory_map_entry_count;

uint64_t executable_physical_base;
uint64_t executable_virtual_base;
uint64_t executable_size;

result_t limine_requests_init() {
  if (LIMINE_BASE_REVISION_SUPPORTED == false) {
    panic_msg("Limine base revision not supported. Please update Limine "
              "to the latest "
              "version.");
    return RESULT_FAILURE(RESULT_ERROR);
  }

  if (limine_req_boot_time.response == NULL ||
      limine_req_hhdm_offset.response == NULL ||
      limine_req_executable_address.response == NULL ||
      limine_req_executable_file.response == NULL ||
      limine_req_memory_map.response == NULL ||
      limine_req_paging_mode.response == NULL) {
    panic_msg("Limine request response was null for one or more requests");
    return RESULT_FAILURE(RESULT_ERROR);
  }

  if (limine_req_framebuffer.response->framebuffer_count < 1) {
    panic_msg("No framebuffers found");
    return RESULT_FAILURE(RESULT_ERROR);
  }

  if (limine_req_paging_mode.response->mode != LIMINE_PAGING_MODE_RISCV_SV39) {
    panic_msg("Paging mode is not SV39");
    return RESULT_FAILURE(RESULT_ERROR);
  }

  if (limine_req_memory_map.response->entry_count < 1) {
    panic_msg("No memory map entries found");
    return RESULT_FAILURE(RESULT_ERROR);
  }

  if (limine_req_memory_map.response->entries == NULL) {
    panic_msg("Memory map entries were null");
    return RESULT_FAILURE(RESULT_ERROR);
  }

  if (limine_req_executable_address.response->physical_base == 0 ||
      limine_req_executable_address.response->virtual_base == 0) {
    panic_msg("Executable address was null");
    return RESULT_FAILURE(RESULT_ERROR);
  }

  if (limine_req_executable_file.response->executable_file == NULL) {
    panic_msg("Executable file was null");
    return RESULT_FAILURE(RESULT_ERROR);
  }

  if (limine_req_executable_file.response->executable_file->size == 0) {
    panic_msg("Executable file size was null");
    return RESULT_FAILURE(RESULT_ERROR);
  }

  if (limine_req_executable_file.response->executable_file->address == NULL) {
    panic_msg("Executable file address was null");
    return RESULT_FAILURE(RESULT_ERROR);
  }

  if (limine_req_hhdm_offset.response->offset == 0) {
    panic_msg("HHDM offset was null");
    return RESULT_FAILURE(RESULT_ERROR);
  }

  if (limine_req_boot_time.response->boot_time == 0) {
    panic_msg("Boot time was null");
    return RESULT_FAILURE(RESULT_ERROR);
  }

  hhdm_offset = limine_req_hhdm_offset.response->offset;

  memory_map_entries = limine_req_memory_map.response->entries;
  memory_map_entry_count = limine_req_memory_map.response->entry_count;

  executable_physical_base =
      limine_req_executable_address.response->physical_base;
  executable_virtual_base =
      limine_req_executable_address.response->virtual_base;
  executable_size = limine_req_executable_file.response->executable_file->size;

  return RESULT_SUCCESS(0);
}

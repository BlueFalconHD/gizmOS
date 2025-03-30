// #include "limine_requests.h"
// #include <lib/panic.h>
// #include <limine.h>

// result_t limine_requests_init() {
//   if (LIMINE_BASE_REVISION_SUPPORTED == false) {
//     panic_msg("Limine base revision not supported. Please update Limine "
//               "to the latest "
//               "version.");
//     return RESULT_FAILURE(RESULT_ERROR);
//   }

//   if (limine_req_boot_time.response == NULL ||
//       limine_req_hhdm_offset.response == NULL ||
//       limine_req_executable_address.response == NULL ||
//       limine_req_executable_file.response == NULL ||
//       limine_req_memory_map.response == NULL ||
//       limine_req_paging_mode.response == NULL) {
//     panic_msg("Limine request response was null for one or more requests");
//     return RESULT_FAILURE(RESULT_ERROR);
//   }

//   if (limine_req_framebuffer.response->framebuffer_count < 1) {
//     panic_msg("No framebuffers found");
//     return RESULT_FAILURE(RESULT_ERROR);
//   }

//   if (limine_req_paging_mode.response->mode != LIMINE_PAGING_MODE_RISCV_SV39)
//   {
//     panic_msg("Paging mode is not SV39");
//     return RESULT_FAILURE(RESULT_ERROR);
//   }

//   if (limine_req_memory_map.response->entry_count < 1) {
//     panic_msg("No memory map entries found");
//     return RESULT_FAILURE(RESULT_ERROR);
//   }

//   if (limine_req_memory_map.response->entries == NULL) {
//     panic_msg("Memory map entries were null");
//     return RESULT_FAILURE(RESULT_ERROR);
//   }

//   if (limine_req_executable_address.response->physical_base == 0 ||
//       limine_req_executable_address.response->virtual_base == 0) {
//     panic_msg("Executable address was null");
//     return RESULT_FAILURE(RESULT_ERROR);
//   }

//   if (limine_req_executable_file.response->executable_file == NULL) {
//     panic_msg("Executable file was null");
//     return RESULT_FAILURE(RESULT_ERROR);
//   }

//   if (limine_req_executable_file.response->executable_file->size == 0) {
//     panic_msg("Executable file size was null");
//     return RESULT_FAILURE(RESULT_ERROR);
//   }

//   if (limine_req_executable_file.response->executable_file->address == NULL)
//   {
//     panic_msg("Executable file address was null");
//     return RESULT_FAILURE(RESULT_ERROR);
//   }

//   if (limine_req_hhdm_offset.response->offset == 0) {
//     panic_msg("HHDM offset was null");
//     return RESULT_FAILURE(RESULT_ERROR);
//   }

//   if (limine_req_boot_time.response->boot_time == 0) {
//     panic_msg("Boot time was null");
//     return RESULT_FAILURE(RESULT_ERROR);
//   }

//   return RESULT_SUCCESS(0);
// }

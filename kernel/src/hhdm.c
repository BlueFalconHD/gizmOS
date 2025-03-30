#include "hhdm.h"
#include "device/term.h"
#include "hcf.h"
#include "lib/panic.h"
#include "limine.h"
#include "memory.h"

#include <stdint.h>

uint64_t hhdm_offset;
uint64_t executable_physical_base;
uint64_t executable_virtual_base;
uint64_t executable_size;

__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_hhdm_request
    hhdm_request = {.id = LIMINE_HHDM_REQUEST, .revision = 0};

__attribute__((used, section(".limine_requests"))) static volatile struct
    limine_executable_address_request executable_address_request = {
        .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST, .revision = 0};

__attribute__((used, section(".limine_requests"))) static volatile struct
    limine_executable_file_request executable_file_request = {
        .id = LIMINE_EXECUTABLE_FILE_REQUEST, .revision = 0};

void hhdm_init() {
  struct limine_hhdm_response *hhdm_response = hhdm_request.response;
  if (hhdm_response == NULL) {
    panic("Response to HHDM request to Limine was null\n");
  } else {
    hhdm_offset = hhdm_response->offset;
  }
}

void executable_address_init() {
  struct limine_executable_address_response *executable_address_response =
      executable_address_request.response;
  if (executable_address_response == NULL) {
    panic("Response to executable address request was null\n");
  } else {
    executable_physical_base = executable_address_response->physical_base;
    executable_virtual_base = executable_address_response->virtual_base;
  }
}

/*
struct limine_executable_file_response {
    uint64_t revision;
    struct limine_file *executable_file;
};
*/

void executable_file_init() {
  struct limine_executable_file_response *executable_file_response =
      executable_file_request.response;
  if (executable_file_response == NULL) {
    panic("Response to executable file request was null\n");
  } else {
    executable_size = executable_file_response->executable_file->size;
  }
}

#include "hhdm.h"
#include "device/term.h"
#include "hcf.h"
#include "limine.h"
#include "memory.h"

#include <stdint.h>

uint64_t hhdm_offset;

__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_hhdm_request
    hhdm_request = {.id = LIMINE_HHDM_REQUEST, .revision = 0};

void hhdm_init() {
  struct limine_hhdm_response *hhdm_response = hhdm_request.response;
  if (hhdm_response == NULL) {
    print_error("Response to HHDM request to Limine was null\n");
    hcf();
  } else {
    hhdm_offset = hhdm_response->offset;
  }
}

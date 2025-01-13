#include "paging_mode.h"
#include "limine.h"
#include "device/term.h"
#include "memory.h"
#include "hcf.h"

__attribute__((used, section(".limine_requests")))
static volatile struct limine_paging_mode_request paging_mode_request = {
    .id = LIMINE_PAGING_MODE_REQUEST,
    .revision = 0,
    .mode = LIMINE_PAGING_MODE_AARCH64_4LVL
};

void paging_mode_init() {

    if (paging_mode_request.response == NULL) {
            print_error("Response to paging mode request was null\n");
            hcf();
    } else {
        // get response from paging mode request
        struct limine_paging_mode_response *paging_mode_response = paging_mode_request.response;

        // ensure mode value is correct
        if (paging_mode_response->mode != LIMINE_PAGING_MODE_AARCH64_4LVL) {
            print_error("Paging mode not set to AArch64 4-level paging\n");
            hcf();
        }
    }
}

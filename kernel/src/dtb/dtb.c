#include "dtb.h"
#include <extern/smoldtb/smoldtb.h>
#include <stdint.h>
// #include <extern/ce-base/ce-base.h>
#include <device/term.h>
#include <limine.h>

__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_dtb_request
    dtb_request = {.id = LIMINE_DTB_REQUEST, .revision = 0};

void hcf() {
  for (;;) {
    asm("wfi");
  }
}

void on_error(const char *why) {
  print_error("smoldtb error:");
  print_error(why);
  hcf();
}

bool init_dtb(uintptr_t start) { return smoldtb_init(start, gizmOS_dtb_ops); }

void dtb_init() {
  struct limine_dtb_response *dtb_response = dtb_request.response;
  if (dtb_response == NULL) {
    print_error("Response to DTB request to Limine was null\n");
  } else {
    bool res = init_dtb((uintptr_t)dtb_response->dtb_ptr);

    if (!res) {
      print_error("Failed to initialize DTB\n");
      hcf();
    }

    dtb_node *root = dtb_find("/");
    if (root == NULL) {
      print_error("Failed to find root node in DTB\n");
      hcf();
    }

    // Search for soc node
    dtb_node *soc = dtb_find_child(root, "soc");

    // Search for serial node
    dtb_node *serial = dtb_find_child(soc, "serial");

    // Get the address of the serial node
    dtb_prop *serial_addr_prop = dtb_find_prop(serial, "address");
    
    
  }
}

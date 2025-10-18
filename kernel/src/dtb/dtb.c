#include "dtb.h"
#include <device/console.h>
#include <extern/smoldtb/smoldtb.h>
#include <lib/panic.h>
#include <lib/print.h>
#include <lib/log.h>
static inline log_t *dtb_log() {
  static log_t *l = NULL;
  if (!l)
    l = g_log_create("dtb", NULL);
  return l;
}
#include <limine.h>
#include <lib/kalloc.h>
#include <stdint.h>

__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_dtb_request
    dtb_request = {.id = LIMINE_DTB_REQUEST, .revision = 0};

void hcf() {
  for (;;) {
    asm("wfi");
  }
}

dtb_node *root_node;

void on_error(const char *why) {
  panic(format("smoldtb error: %{type: str}\n", why));
}

bool init_dtb(uintptr_t start) { return smoldtb_init(start, gizmOS_dtb_ops); }

void dtb_init() {
  struct limine_dtb_response *dtb_response = dtb_request.response;
  if (dtb_response == NULL) {
    panic("Response to DTB request to Limine was null\n");
  }

  bool res = init_dtb((uintptr_t)dtb_response->dtb_ptr);

  if (!res) {
    panic("Failed to initialize DTB\n");
  }

  root_node = dtb_find("/");
  if (root_node == NULL) {
    panic("Failed to find root node\n");
  }
}

void dtb_dostuff() {
  // Search for soc node
  dtb_node *soc = dtb_find_child(root_node, "soc");
  if (!soc) {
    panic("Failed to find soc node\n");
  }

  // Get address-cells and size-cells from parent
  size_t addr_cells = dtb_get_addr_cells_for(soc);
  size_t size_cells = dtb_get_size_cells_for(soc);

  LOG_INFO(dtb_log(), "Address cells: %{type: int}, Size cells: %{type: int}", addr_cells, size_cells);

  // Search for serial node
  dtb_node *serial = dtb_find_child(soc, "serial");
  if (!serial) {
    panic("Failed to find serial node\n");
  }

  // Get the address of the serial node
  dtb_prop *reg_prop = dtb_find_prop(serial, "reg");
  if (!reg_prop) {
    panic("Failed to find reg property\n");
  }

  // First call to determine how many values we have
  size_t pair_count =
      dtb_read_prop_2(reg_prop, (dtb_pair){addr_cells, size_cells}, NULL);
  if (pair_count == 0) {
    panic("Failed to read reg property values\n");
  }

  // Allocate memory for the values
  dtb_pair *reg_values = (dtb_pair *)kalloc(sizeof(dtb_pair) * pair_count);
  if (!reg_values) {
    panic("Failed to allocate memory for reg values\n");
  }

  // Second call to actually read the values
  dtb_read_prop_2(reg_prop, (dtb_pair){addr_cells, size_cells}, reg_values);

  // The address is in the first pair's 'a' field
  LOG_INFO(dtb_log(), "Serial address: 0x%{type: hex}", reg_values[0].a);

  // The size is in the first pair's 'b' field
  LOG_INFO(dtb_log(), "Serial size: 0x%{type: hex}", reg_values[0].b);

  // Don't forget to free the allocated memory
  kfree(reg_values);
}

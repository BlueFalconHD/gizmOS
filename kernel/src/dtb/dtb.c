#include "dtb.h"
#include <stdint.h>
#include "../memory.h"

bool verify_magic(struct fdt_header *header) {
    return header->magic == FDT_MAGIC_LE;
}

uint32_t size_mem_rsvmap(struct fdt_header *header) {
    uint32_t total_size = swap_uint32(header->totalsize);
    uint32_t size_dt_struct = swap_uint32(header->size_dt_struct);
    uint32_t size_dt_strings = swap_uint32(header->size_dt_strings);

    return total_size - size_dt_struct - size_dt_strings;
}

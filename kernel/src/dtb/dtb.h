#ifndef DTB_H
#define DTB_H 1

#include <stdint.h>
#include <stdbool.h>

#define FDT_MAGIC_BE 0xd00dfeed
#define FDT_MAGIC_LE 0xedfe0dd0

#define FDT_HEADER_SIZE sizeof(struct fdt_header)

struct fdt_header {
    uint32_t magic;             // NOTE: big-endian
    uint32_t totalsize;         // NOTE: big-endian
    uint32_t off_dt_struct;     // NOTE: big-endian
    uint32_t off_dt_strings;    // NOTE: big-endian
    uint32_t off_mem_rsvmap;    // NOTE: big-endian
    uint32_t version;           // NOTE: big-endian
    uint32_t last_comp_version; // NOTE: big-endian
    uint32_t boot_cpuid_phys;   // NOTE: big-endian
    uint32_t size_dt_strings;   // NOTE: big-endian
    uint32_t size_dt_struct;    // NOTE: big-endian
};

bool verify_magic(struct fdt_header *header);

uint32_t size_mem_rsvmap(struct fdt_header *header);

#define FDT_RESERVE_ENTRY_SIZE sizeof(struct fdt_reserve_entry)

// loc = fdt + fdt_header->off_mem_rsvmap,
// size = fdt_header->totalsize - FDT_HEADER_SIZE - fdt_header->size_dt_struct - fdt_header->size_dt_strings,
// number of entries = size / FDT_RESERVE_ENTRY_SIZE
struct fdt_reserve_entry {
    uint64_t address;
    uint64_t size;
};

#endif /* DTB_H */

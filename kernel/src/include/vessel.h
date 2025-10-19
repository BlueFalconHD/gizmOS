#pragma once

#include <lib/types.h>

// Vessel file format (packed, little-endian)

// Magic is the ASCII string "vessel  " (8 bytes, space-padded)
#define VESSEL_MAGIC_U64 ((uint64_t)0x20206c6573736576ULL)

// Version encoding: upper 16 bits = major, lower 16 bits = minor
#define VESSEL_VERSION(maj, min) ((((uint32_t)(maj)) << 16) | ((uint32_t)(min)))

// Command types
#define VESSEL_CMD_SEGMENT      1
#define VESSEL_CMD_ENTRY_POINT  2

// Segment flags (bitfield)
#define VESSEL_SEG_R  (1u << 0)
#define VESSEL_SEG_W  (1u << 1)
#define VESSEL_SEG_X  (1u << 2)

typedef struct __attribute__((packed)) {
  uint64_t magic;         // VESSEL_MAGIC_U64
  uint32_t version;       // VESSEL_VERSION
  uint32_t commands_size; // bytes of command list following this header
} vessel_hdr_t;

typedef struct __attribute__((packed)) {
  uint32_t type; // VESSEL_CMD_*
  uint32_t size; // size including this header
} vessel_cmd_t;

typedef struct __attribute__((packed)) {
  uint32_t type;        // VESSEL_CMD_SEGMENT
  uint32_t size;        // sizeof(struct vessel_cmd_segment)
  uint64_t vaddr;       // virtual address to map the segment
  uint64_t mem_size;    // bytes to reserve/map in memory
  uint64_t file_offset; // offset within the file where payload begins
  uint64_t file_size;   // payload bytes in file
  uint32_t flags;       // VESSEL_SEG_* flags
  uint32_t reserved;    // padding
} vessel_cmd_segment_t;

typedef struct __attribute__((packed)) {
  uint32_t type;  // VESSEL_CMD_ENTRY_POINT
  uint32_t size;  // sizeof(struct vessel_cmd_entry_point)
  uint64_t entry; // virtual entry point
} vessel_cmd_entry_point_t;



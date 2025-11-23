// ObjectFS on-disk format definitions (read-only MVP)
#pragma once

#include <lib/types.h>

#define OBJFS_MAGIC_STR "OBJFS1"
#define OBJFS_MAGIC_LEN 6

#define OBJFS_BLOCK_SIZE_DEFAULT 4096u

// Object kinds (on-disk)
typedef enum {
  OBJFS_OBJ_UNKNOWN = 0,
  OBJFS_OBJ_FILE = 1,
  OBJFS_OBJ_DIR = 2,
  OBJFS_OBJ_REFERENCE = 3,
} objfs_obj_kind_t;

// Superblock at LBA 0 (first block)
typedef struct __attribute__((packed)) {
  char     magic[8];             // "OBJFS1" + zeros
  uint32_t version;              // format version
  uint32_t block_size;           // bytes per block (e.g., 4096)
  uint64_t root_object_id;       // id of root directory object
  // Object table (fixed-size descriptors)
  uint64_t object_table_start;   // block index where object table begins
  uint64_t object_table_blocks;  // number of blocks in object table
  // Free bitmap (phase 2 - allocator), still present for layout
  uint64_t free_map_start;       // block index of free map (optional in RO)
  uint64_t free_map_blocks;      // number of blocks in free map
  // Content region (blocks containing file data extents)
  uint64_t content_start;        // first block usable for content
  uint64_t _reserved64[8];       // future
} objfs_superblock_t;

// Fixed-size object descriptor stored in object table.
typedef struct __attribute__((packed)) {
  uint64_t id;                   // unique object identifier
  uint16_t mode;                 // permission bits
  uint8_t  kind;                 // objfs_obj_kind_t
  uint8_t  flags;                // reserved for future
  uint32_t uid;                  // owner
  uint32_t gid;                  // group
  uint32_t nlink;                // link count (dir entries + refs)
  uint64_t size;                 // file size in bytes (or dir metadata size)
  uint64_t atime;                // timestamps (ns since epoch or system-defined)
  uint64_t mtime;
  uint64_t ctime;
  // When kind==REFERENCE, redirect to this target id
  uint64_t target_id;
  // Pointers into other structures (block indexes relative to disk start)
  uint64_t children_idx;         // head block of children index (dirs only)
  uint64_t attrs_head;           // head block of attributes chain
  // Single-extent content mapping for MVP read support
  uint64_t data_start_block;     // starting block of file contents
  uint32_t data_num_blocks;      // number of contiguous blocks
  uint32_t _pad32;
  uint64_t _reserved64[8];
} objfs_object_disk_t;

// Children index block header
typedef struct __attribute__((packed)) {
  uint32_t count;                // number of entries in this block
  uint32_t _pad;
  uint64_t next_block;           // 0 if end; else block index of next children block
} objfs_children_block_hdr_t;

// A single child entry in a children block
typedef struct __attribute__((packed)) {
  uint8_t  name_len;             // <= 63
  uint8_t  type;                 // objfs_obj_kind_t (for convenience)
  uint16_t _pad16;
  char     name[64];             // UTF-8; not null-terminated if name_len==64
  uint64_t child_id;             // referenced object id
} objfs_child_entry_t;

// Attributes block header
typedef struct __attribute__((packed)) {
  uint32_t count;                // number of attribute entries in this block
  uint32_t _pad;
  uint64_t next_block;           // 0 if end; else block index of next attrs block
} objfs_attrs_block_hdr_t;

// Attribute value types
typedef enum {
  OBJFS_ATTR_STR = 0,
  OBJFS_ATTR_INT = 1,
  OBJFS_ATTR_BOOL = 2,
} objfs_attr_type_t;

// Variable-length attribute entry header (payload follows)
typedef struct __attribute__((packed)) {
  uint8_t  key_len;              // <= 63
  uint8_t  type;                 // objfs_attr_type_t
  uint16_t vlen;                 // bytes of value for STR; 0 for INT/BOOL
  char     key[64];              // UTF-8; not null-terminated if key_len==64
  // Followed by:
  //   if STR:  vlen bytes of string data (not null-terminated)
  //   if INT:  int64_t value
  //   if BOOL: uint8_t (0/1)
} objfs_attr_entry_hdr_t;



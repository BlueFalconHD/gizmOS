#!/usr/bin/env python3
import argparse
import os
import struct
from pathlib import Path


BLOCK_SIZE = 4096


class Super:
    FORMAT = "<QIIIIIIIIIII"

    def __init__(self):
        self.magic = 0x3153465A47  # 'GZFS1' little-endian
        self.block_size = BLOCK_SIZE
        self.total_blocks = 0
        self.inode_count = 0
        self.journal_start = 0
        self.journal_len = 0
        self.inode_start = 0
        self.inode_blocks = 0
        self.bitmap_start = 0
        self.bitmap_blocks = 0
        self.data_start = 0
        self.reserved = 0

    def pack(self) -> bytes:
        return struct.pack(
            self.FORMAT,
            self.magic,
            self.block_size,
            self.total_blocks,
            self.inode_count,
            self.journal_start,
            self.journal_len,
            self.inode_start,
            self.inode_blocks,
            self.bitmap_start,
            self.bitmap_blocks,
            self.data_start,
            self.reserved,
        )


INODE_DISK_FORMAT = "<HHIQQQQ" + "I" * 12 + "IIIII" + "88s"
INODE_DISK_SIZE = struct.calcsize(INODE_DISK_FORMAT)


def ceil_div(a, b):
    return (a + b - 1) // b


def collect_files(diskdir: Path):
    files = []
    for p in sorted(diskdir.iterdir()):
        if p.is_file():
            data = p.read_bytes()
            files.append((p.name, data, {}))
        elif p.is_dir() and p.name.upper().endswith(".VES"):
            # Treat .VES directories like resource bundles? Keep as file entries via vessel later.
            data = p.read_bytes() if p.is_file() else b""
            files.append((p.name, data, {}))
    # collect forks from optional .forks/<filename>/*
    forks_root = diskdir / ".forks"
    if forks_root.exists() and forks_root.is_dir():
        # Map base filename -> dict of forkname -> bytes
        fork_map = {}
        for base in sorted(forks_root.iterdir()):
            if not base.is_dir():
                continue
            base_name = base.name
            for forkfile in sorted(base.iterdir()):
                if forkfile.is_file():
                    fdata = forkfile.read_bytes()
                    fork_map.setdefault(base_name, {})[forkfile.name] = fdata
        # merge into files list
        merged = []
        for name, data, _forks in files:
            merged.append((name, data, fork_map.get(name, {})))
        files = merged
    return files


def write_at(f, blkno: int, data: bytes):
    f.seek(blkno * BLOCK_SIZE)
    f.write(data)


def build_image(out_path: Path, size_mb: int, diskdir: Path):
    files = collect_files(diskdir)

    # Layout calculation
    num_inodes = 1 + len(files)  # inode 1 = root
    ipb = BLOCK_SIZE // INODE_DISK_SIZE
    inode_blocks = ceil_div(num_inodes, ipb)

    # Directory entries (fixed 72 bytes each)
    dirent_size = 72
    dir_size = len(files) * dirent_size
    dir_blocks = ceil_div(dir_size, BLOCK_SIZE)

    # File block allocation plan
    file_plans = []
    data_blocks_needed = dir_blocks
    for name, data, forks in files:
        nblocks = ceil_div(len(data), BLOCK_SIZE)
        need_indirect = max(0, nblocks - 12)
        ind_blocks = 1 if need_indirect > 0 else 0
        # count fork data blocks
        fork_plans = []
        fork_tbl_blocks = 0
        if forks:
            # one block for table
            fork_tbl_blocks = 1
            for fname, fdata in forks.items():
                fnblocks = ceil_div(len(fdata), BLOCK_SIZE)
                find_blocks = 1 if max(0, fnblocks - 12) > 0 else 0
                fork_plans.append((fname, fdata, fnblocks, find_blocks))
        # increment data blocks
        data_blocks_needed += nblocks + ind_blocks + fork_tbl_blocks
        for _fname, _fdata, fnblocks, find_blocks in fork_plans:
            data_blocks_needed += fnblocks + find_blocks
        file_plans.append(
            (name, data, nblocks, ind_blocks, forks, fork_plans, fork_tbl_blocks)
        )

    # Bitmap blocks: 1 bit per data block
    bits_per_block = BLOCK_SIZE * 8
    bitmap_blocks = ceil_div(data_blocks_needed + 1, bits_per_block)

    sb = Super()
    sb.inode_start = 1
    sb.inode_blocks = inode_blocks
    sb.bitmap_start = sb.inode_start + sb.inode_blocks
    sb.bitmap_blocks = bitmap_blocks
    sb.data_start = sb.bitmap_start + sb.bitmap_blocks

    total_blocks = (size_mb * 1024 * 1024) // BLOCK_SIZE
    if total_blocks == 0:
        total_blocks = sb.data_start + data_blocks_needed
    sb.total_blocks = total_blocks
    sb.inode_count = num_inodes

    # Create/size the image
    with open(out_path, "wb") as f:
        f.seek(total_blocks * BLOCK_SIZE - 1)
        f.write(b"\0")

    # Prepare inode table buffer
    inode_tbl = bytearray(inode_blocks * BLOCK_SIZE)

    # Data region allocation cursor (relative block index within data region)
    alloc_cursor = 1

    # Build root directory data
    root_blocks = []
    dirbuf = bytearray(dir_blocks * BLOCK_SIZE)
    off = 0
    inum = 2  # start assigning file inodes from 2
    file_inode_numbers = {}
    for (
        name,
        data,
        nblocks,
        ind_blocks,
        forks,
        fork_plans,
        fork_tbl_blocks,
    ) in file_plans:
        enc = name.encode("utf-8")[:64]
        namelen = len(enc)
        de = struct.pack("<IBBH", inum, 1, namelen, 0) + enc.ljust(64, b"\0")
        dirbuf[off : off + dirent_size] = de
        off += dirent_size
        file_inode_numbers[name] = inum
        inum += 1

    # Allocate blocks for directory
    for _ in range(dir_blocks):
        root_blocks.append(alloc_cursor)
        alloc_cursor += 1

    # Build and write file data + possible indirect blocks
    files_block_lists = {}
    files_fork_tables = {}
    with open(out_path, "r+b") as f:
        # write super later, after everything else
        # write directory data blocks
        for i, rb in enumerate(root_blocks):
            blk_data = dirbuf[i * BLOCK_SIZE : (i + 1) * BLOCK_SIZE]
            write_at(f, sb.data_start + rb, blk_data)

        # files
        for (
            name,
            data,
            nblocks,
            ind_blocks,
            forks,
            fork_plans,
            fork_tbl_blocks,
        ) in file_plans:
            blocks = []
            for i in range(nblocks):
                blocks.append(alloc_cursor)
                blk_slice = data[i * BLOCK_SIZE : (i + 1) * BLOCK_SIZE]
                write_at(
                    f, sb.data_start + alloc_cursor, blk_slice.ljust(BLOCK_SIZE, b"\0")
                )
                alloc_cursor += 1
            if ind_blocks:
                # allocate one block for indirect array
                ind_blk = alloc_cursor
                alloc_cursor += 1
                # prepare array of u32 block indices (relative to data_start)
                arr = blocks[12:]
                arr_bytes = bytearray(BLOCK_SIZE)
                for i, bno in enumerate(arr):
                    struct.pack_into("<I", arr_bytes, i * 4, bno)
                write_at(f, sb.data_start + ind_blk, arr_bytes)
                files_block_lists[name] = (blocks, ind_blk)
            else:
                files_block_lists[name] = (blocks, 0)

            # Fork table and fork data
            fork_tbl_rel = 0
            if fork_plans:
                # Reserve a block for the fork table itself, then allocate
                # fork data blocks after it so fork entries don't point at
                # the table header.
                fork_tbl_rel = alloc_cursor
                alloc_cursor += 1
                # Build header + entries (fixed size)
                hdr = struct.pack("<II", len(fork_plans), 0)
                table_bytes = bytearray(BLOCK_SIZE)
                table_bytes[: len(hdr)] = hdr
                pos = 8
                # write fork data first to know their blocks
                fork_entries = []
                fork_used_blocks = set()
                for fname, fdata, fnblocks, find_blocks in fork_plans:
                    fblocks = []
                    for j in range(fnblocks):
                        fblocks.append(alloc_cursor)
                        blk_slice = fdata[j * BLOCK_SIZE : (j + 1) * BLOCK_SIZE]
                        write_at(
                            f,
                            sb.data_start + alloc_cursor,
                            blk_slice.ljust(BLOCK_SIZE, b"\0"),
                        )
                        fork_used_blocks.add(alloc_cursor)
                        alloc_cursor += 1
                    if find_blocks:
                        ind_b = alloc_cursor
                        alloc_cursor += 1
                        arr = fblocks[12:]
                        arr_bytes = bytearray(BLOCK_SIZE)
                        for i, bno in enumerate(arr):
                            struct.pack_into("<I", arr_bytes, i * 4, bno)
                        write_at(f, sb.data_start + ind_b, arr_bytes)
                    else:
                        ind_b = 0
                    fork_entries.append((fname, len(fdata), fblocks[:12], ind_b))
                # now write entries into table
                for fname, fsize, fdir12, ind_b in fork_entries:
                    encn = fname.encode("utf-8")[:64]
                    namelen = len(encn)
                    entry_bytes = bytearray(
                        struct.calcsize("<BBH64sQ" + "I" * 12 + "II")
                    )
                    struct.pack_into("<BBH", entry_bytes, 0, namelen, 0, 0)
                    struct.pack_into("<64s", entry_bytes, 4, encn.ljust(64, b"\0"))
                    struct.pack_into("<Q", entry_bytes, 68, fsize)
                    offp = 76
                    for v in fdir12:
                        struct.pack_into("<I", entry_bytes, offp, v)
                        offp += 4
                    # pad remaining direct entries
                    for _ in range(12 - len(fdir12)):
                        struct.pack_into("<I", entry_bytes, offp, 0)
                        offp += 4
                    struct.pack_into("<I", entry_bytes, offp, ind_b)
                    offp += 4
                    struct.pack_into("<I", entry_bytes, offp, 0)
                    offp += 4
                    # place into table
                    endp = pos + len(entry_bytes)
                    if endp > BLOCK_SIZE:
                        raise SystemExit("Fork table overflow for " + name)
                    table_bytes[pos:endp] = entry_bytes
                    pos = endp
                # write table
                write_at(f, sb.data_start + fork_tbl_rel, table_bytes)
            files_fork_tables[name] = fork_tbl_rel

        # Bitmap
        bitmap_total_bits = data_blocks_needed
        bitmap = bytearray(bitmap_blocks * BLOCK_SIZE)
        # mark allocated blocks: dir + files + indirects
        used = set(root_blocks)
        for name, (blocks, ind_blk) in files_block_lists.items():
            used.update(blocks)
            if ind_blk:
                used.add(ind_blk)
            fork_tbl_rel = files_fork_tables.get(name, 0)
            if fork_tbl_rel:
                used.add(fork_tbl_rel)
        for bno in used:
            byte_index = bno // 8
            bit_index = bno % 8
            bitmap[byte_index] |= 1 << bit_index
        # write bitmap blocks
        for i in range(bitmap_blocks):
            chunk = bitmap[i * BLOCK_SIZE : (i + 1) * BLOCK_SIZE]
            write_at(f, sb.bitmap_start + i, chunk)

        # Inodes
        def write_inode(
            slot_index: int,
            mode: int,
            ftype: int,
            nlink: int,
            size: int,
            direct_list,
            indirect1: int,
            indirect2: int = 0,
            reserved0: int = 0,
        ):
            base = slot_index * INODE_DISK_SIZE
            # pack inode
            atime = mtime = ctime = 0
            direct = list(direct_list) + [0] * (12 - len(direct_list))
            packed = struct.pack(
                INODE_DISK_FORMAT,
                mode & 0xFFFF,
                ftype & 0xFFFF,
                nlink,
                size,
                atime,
                mtime,
                ctime,
                *direct,
                indirect1,
                indirect2,
                reserved0,
                0,
                0,
                b"\0" * 88,
            )
            inode_tbl[base : base + INODE_DISK_SIZE] = packed

        # inode 1: root dir
        write_inode(0, 0o755, 2, 1, dir_size, root_blocks[:12], 0)

        # file inodes
        slot = 1
        for (
            name,
            data,
            nblocks,
            ind_blocks,
            forks,
            fork_plans,
            fork_tbl_blocks,
        ) in file_plans:
            blocks, ind_blk = files_block_lists[name]
            size = len(data)
            direct_list = blocks[:12]
            reserved0 = files_fork_tables.get(name, 0)
            write_inode(slot, 0o644, 1, 1, size, direct_list, ind_blk, 0, reserved0)
            slot += 1

        # write inode table blocks
        for i in range(inode_blocks):
            chunk = inode_tbl[i * BLOCK_SIZE : (i + 1) * BLOCK_SIZE]
            write_at(f, sb.inode_start + i, chunk)

        # finally, write superblock
        write_at(f, 0, sb.pack().ljust(BLOCK_SIZE, b"\0"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--diskdir", default="disk", help="Directory tree to pack as root")
    ap.add_argument("--out", default="data.img", help="Output image path")
    ap.add_argument("--size-mb", type=int, default=64, help="Total image size (MB)")
    args = ap.parse_args()

    diskdir = Path(args.diskdir)
    if not diskdir.exists() or not diskdir.is_dir():
        raise SystemExit(f"Disk dir not found: {diskdir}")
    out = Path(args.out)
    build_image(out, args.size_mb, diskdir)
    print(f"[mkfs_gzfs] Wrote {out} ({args.size_mb} MiB) with root from {diskdir}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
import struct
import sys

MAGIC = 0x20206c6573736576  # "vessel  "

CMD_SEGMENT = 1
CMD_ENTRY   = 2

SEG_R = 1 << 0
SEG_W = 1 << 1
SEG_X = 1 << 2

def align_up(x, a):
    return (x + a - 1) & ~(a - 1)

def pack_single(bin_path, out_path, vaddr=0x0, flags=SEG_R | SEG_W | SEG_X, entry=0x0):
    with open(bin_path, 'rb') as f:
        payload = f.read()

    page = 4096
    payload_size = len(payload)
    # Reserve extra anonymous zeroed memory after the payload to cover .bss and
    # other zero-initialized globals that live beyond the raw file contents.
    #
    # Without this, user code that accesses globals in the BSS region (whose
    # virtual addresses may be higher than the last byte in the binary) can
    # fault when the loader has only mapped [0, payload_size).
    #
    # This is a conservative fixed slack; the kernel's Vessel loader will map
    # [vaddr, vaddr + mem_size) and the extra region will naturally be zeroed
    # by buddy_alloc_page().
    bss_slack = 1024 * 1024  # 1 MiB of zeroed space for BSS
    mem_size = align_up(payload_size + bss_slack, page)

    # Build command list in-memory
    cmds = bytearray()

    # segment command
    seg_fmt = '<IIQQQQII'
    seg_size = struct.calcsize(seg_fmt)
    # placeholder file_offset, we compute after header+cmds size known
    file_offset = 0
    seg = struct.pack(seg_fmt,
        CMD_SEGMENT,
        seg_size,
        vaddr,
        mem_size,
        file_offset,
        payload_size,
        flags,
        0)
    cmds += seg

    # entry command
    ent_fmt = '<IIQ'
    ent_size = struct.calcsize(ent_fmt)
    ent = struct.pack(ent_fmt, CMD_ENTRY, ent_size, entry)
    cmds += ent

    header_fmt = '<QII'
    header_size = struct.calcsize(header_fmt)
    commands_size = len(cmds)

    # Now compute file_offset for payload
    payload_offset = header_size + commands_size

    # Re-pack segment command with correct file_offset
    seg = struct.pack(seg_fmt,
        CMD_SEGMENT,
        seg_size,
        vaddr,
        mem_size,
        payload_offset,
        payload_size,
        flags,
        0)
    cmds[:seg_size] = seg

    with open(out_path, 'wb') as out:
        out.write(struct.pack(header_fmt, MAGIC, (1 << 16) | 0, commands_size))
        out.write(cmds)
        out.write(payload)

def main(argv):
    if len(argv) < 3:
        print('Usage: vessel_pack.py <in.bin> <out.vessel> [entry_hex]')
        return 1
    bin_path = argv[1]
    out_path = argv[2]
    entry = int(argv[3], 16) if len(argv) > 3 else 0
    # Single flat segment for now: make it readable, writable, and executable.
    # Writable is required so that globals/BSS in the same segment can be
    # modified without triggering store page faults in user space.
    pack_single(bin_path, out_path, vaddr=0x0, flags=SEG_R | SEG_W | SEG_X, entry=entry)
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))



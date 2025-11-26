#!/usr/bin/env python3
import struct
import sys
import os

MAGIC = 0x20206c6573736576  # "vessel  "

CMD_SEGMENT = 1
CMD_ENTRY   = 2

SEG_R = 1 << 0
SEG_W = 1 << 1
SEG_X = 1 << 2

def align_up(x, a):
    return (x + a - 1) & ~(a - 1)

def build_vessel_bytes(bin_path, vaddr=0x0, flags=SEG_R | SEG_W | SEG_X, entry=0x0) -> bytes:
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

    out = bytearray()
    out += struct.pack(header_fmt, MAGIC, (1 << 16) | 0, commands_size)
    out += cmds
    out += payload
    return bytes(out)

def main(argv):
    if len(argv) < 3:
        print('Usage: vessel_pack.py <in.bin> <out.{vessel|obj}> [entry_hex]')
        return 1
    bin_path = argv[1]
    out_path = argv[2]
    entry = int(argv[3], 16) if len(argv) > 3 else 0
    # Build vessel bytes (single flat segment, RWX to support BSS/globals)
    vessel_bytes = build_vessel_bytes(bin_path, vaddr=0x0, flags=SEG_R | SEG_W | SEG_X, entry=entry)

    # If output ends with .obj, emit an ObjectFS container directory:
    #   <out>.obj/
    #     contents           (human-readable metadata)
    #     attributes.yaml    (typed attributes)
    #     subobjects/
    #       exe              (the actual vessel file)
    if out_path.endswith('.obj'):
        obj_dir = out_path
        sub_dir = os.path.join(obj_dir, 'subobjects')
        os.makedirs(sub_dir, exist_ok=True)
        # write subobject exe
        exe_path = os.path.join(sub_dir, 'exe')
        with open(exe_path, 'wb') as f:
            f.write(vessel_bytes)
        # write attributes.yaml - mark this object as a "vessel" container
        attrs_path = os.path.join(obj_dir, 'attributes.yaml')
        with open(attrs_path, 'w', encoding='utf-8') as f:
            f.write('vessel: true\n')
        # write contents (metadata)
        meta_path = os.path.join(obj_dir, 'contents')
        flags_str = []
        if SEG_R & (SEG_R | SEG_W | SEG_X): flags_str.append('R')
        if SEG_W & (SEG_R | SEG_W | SEG_X): flags_str.append('W')
        if SEG_X & (SEG_R | SEG_W | SEG_X): flags_str.append('X')
        with open(meta_path, 'w', encoding='utf-8') as f:
            f.write('format: vessel\n')
            f.write(f'entry: 0x{entry:016x}\n')
            f.write(f'payload_bytes: {len(vessel_bytes)}\n')
            f.write(f'flags: {"".join(flags_str)}\n')
        return 0

    # Otherwise, maintain legacy behavior and write a flat .vessel file.
    with open(out_path, 'wb') as out:
        out.write(vessel_bytes)
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))



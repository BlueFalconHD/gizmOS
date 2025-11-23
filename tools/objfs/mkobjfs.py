#!/usr/bin/env python3
import argparse
import os
import struct
import sys
from pathlib import Path

BS = 4096
RESERVED_OBJ_SLOTS = 256

OBJ_KIND_UNKNOWN = 0
OBJ_KIND_FILE = 1
OBJ_KIND_DIR = 2
OBJ_KIND_REFERENCE = 3

def pack_super(sb):
    magic = b'OBJFS1\x00\x00'
    return struct.pack('<8sIIQQQQQQQQ8Q',
        magic,
        sb['version'],
        sb['block_size'],
        sb['root_object_id'],
        sb['object_table_start'],
        sb['object_table_blocks'],
        sb['free_map_start'],
        sb['free_map_blocks'],
        sb['content_start'],
        0,0,0,0,0,0,0,0 # reserved
    )

def pack_object(o):
    return struct.pack('<QHBBIIIQQQQQQQI I8Q',
        o['id'],
        o.get('mode',0o644),
        o['kind'],
        o.get('flags',0),
        o.get('uid',0),
        o.get('gid',0),
        o.get('nlink',1),
        o.get('size',0),
        o.get('atime',0),
        o.get('mtime',0),
        o.get('ctime',0),
        o.get('target_id',0),
        o.get('children_idx',0),
        o.get('attrs_head',0),
        o.get('data_start_block',0),
        o.get('data_num_blocks',0),
        0, # _pad32
        0,0,0,0,0,0,0,0 # reserved
    )

def pack_children_block(entries, next_block):
    hdr = struct.pack('<IIQ', len(entries), 0, next_block)
    body = b''
    for name, kind, child_id in entries:
        n = name.encode('utf-8')
        n = n[:64]
        name_len = len(n)
        n_padded = n + b'\x00'*(64-len(n))
        body += struct.pack('<BBH64sQ', name_len, kind, 0, n_padded, child_id)
    blk = hdr + body
    if len(blk) > BS:
        raise RuntimeError('children block overflow')
    return blk + b'\x00'*(BS-len(blk))

class Builder:
    def __init__(self, diskdir):
        self.diskdir = Path(diskdir)
        self.nodes = []  # list of dicts {id, path, kind, children, size, file_path}
        self.path_to_id = {}
        self.children_entries = {}  # id -> list of (name, kind, child_id)
        self.objects = []  # filled later with layout
        self.children_blocks = []  # (block_index, entries, next_block)
        self.content_blocks = []   # (block_index, data)
        self.next_free_block = 0
        self.object_table_start = 0
        self.object_table_blocks = 0
        self.free_map_start = 0
        self.free_map_blocks = 0
        self.content_region_start = 0
        self.total_blocks = 0
        self.used_blocks = set()

    def scan(self):
        # Assign IDs breadth-first by walking diskdir
        root_id = 0
        self._add_node(self.diskdir, root_id, OBJ_KIND_DIR)
        for dirpath, dirnames, filenames in os.walk(self.diskdir):
            dirpath = Path(dirpath)
            parent_id = self.path_to_id[dirpath]
            # children entries
            entries = []
            for d in sorted(dirnames):
                p = dirpath / d
                oid = len(self.nodes)
                self._add_node(p, oid, OBJ_KIND_DIR)
                entries.append((d, OBJ_KIND_DIR, oid))
            for f in sorted(filenames):
                p = dirpath / f
                oid = len(self.nodes)
                size = p.stat().st_size
                self._add_node(p, oid, OBJ_KIND_FILE, size=size)
                entries.append((f, OBJ_KIND_FILE, oid))
            self.children_entries[parent_id] = entries

    def _add_node(self, path, oid, kind, size=0):
        self.nodes.append({
            'id': oid,
            'path': Path(path),
            'kind': kind,
            'size': size,
        })
        self.path_to_id[Path(path)] = oid

    def layout(self, total_blocks):
        self.total_blocks = total_blocks
        nobj = len(self.nodes)
        per = BS //  (8+2+1+1+4+4+4+8+8+8+8+8+8+8+8+4+4+8*8)  # sizeof(objfs_object_disk_t)
        if per == 0:
            raise RuntimeError('object too large for block size')
        n_total_objs = nobj + RESERVED_OBJ_SLOTS
        obj_blocks = (n_total_objs + per - 1) // per
        self.object_table_start = 1  # block 0 is superblock
        self.object_table_blocks = obj_blocks
        # place free map after object table; cover entire device
        bits = total_blocks
        self.free_map_blocks = (bits + (BS*8) - 1) // (BS*8)
        self.free_map_start = self.object_table_start + self.object_table_blocks
        # content/metadata region after free map
        self.content_region_start = self.free_map_start + self.free_map_blocks
        self.next_free_block = self.content_region_start
        # mark initial used: superblock + object table + freemap
        for b in range(0, self.content_region_start):
            self.used_blocks.add(b)
        # children blocks
        for node in self.nodes:
            if node['kind'] != OBJ_KIND_DIR:
                continue
            ents = self.children_entries.get(node['id'], [])
            # one block should be enough for MVP; if not, chain single block anyway
            blk_idx = self.next_free_block
            self.next_free_block += 1
            self.children_blocks.append((blk_idx, ents, 0))
            node['children_idx'] = blk_idx if ents else 0
            self.used_blocks.add(blk_idx)
        # content blocks
        for node in self.nodes:
            if node['kind'] != OBJ_KIND_FILE:
                continue
            size = node['size']
            if size == 0:
                node['data_start_block'] = 0
                node['data_num_blocks'] = 0
                continue
            nblocks = (size + BS - 1) // BS
            start = self.next_free_block
            node['data_start_block'] = start
            node['data_num_blocks'] = nblocks
            self.next_free_block += nblocks
            # we will fill data later
            for bi in range(nblocks):
                self.used_blocks.add(start + bi)

        self.objects = []
        for node in self.nodes:
            o = {
                'id': node['id'],
                'kind': node['kind'],
                'size': node.get('size',0),
                'children_idx': node.get('children_idx', 0),
                'attrs_head': 0,
                'data_start_block': node.get('data_start_block', 0),
                'data_num_blocks': node.get('data_num_blocks', 0),
                'nlink': 1,
                'mode': 0o755 if node['kind'] == OBJ_KIND_DIR else 0o644,
            }
            self.objects.append(o)

        sb = {
            'version': 1,
            'block_size': BS,
            'root_object_id': 0,
            'object_table_start': self.object_table_start,
            'object_table_blocks': self.object_table_blocks,
            'free_map_start': self.free_map_start,
            'free_map_blocks': self.free_map_blocks,
            'content_start': self.content_region_start,
        }
        return sb

    def write(self, out_path, total_blocks):
        sb = self.layout(total_blocks)
        with open(out_path, 'wb') as f:
            f.truncate(total_blocks * BS)
        with open(out_path, 'r+b') as f:
            # superblock
            f.seek(0)
            f.write(pack_super(sb).ljust(BS, b'\x00'))
            # object table
            idx = self.object_table_start
            per = BS // (8+2+1+1+4+4+4+8+8+8+8+8+8+8+8+4+4+8*8)
            off = 0
            cur_block = idx
            # write all objects (including reserved empty slots)
            all_objs = self.objects + [ {'id': len(self.objects)+i, 'kind': OBJ_KIND_UNKNOWN, 'size': 0, 'children_idx': 0, 'attrs_head': 0, 'data_start_block': 0, 'data_num_blocks': 0, 'nlink': 0, 'mode': 0} for i in range(RESERVED_OBJ_SLOTS) ]
            for i, o in enumerate(all_objs):
                if (i % per) == 0:
                    # start new block
                    f.seek(cur_block * BS)
                    off = 0
                    cur_block += 1
                f.seek((cur_block - 1) * BS + off)
                f.write(pack_object(o))
                off += (8+2+1+1+4+4+4+8+8+8+8+8+8+8+8+4+4+8*8)
            # children blocks
            for blk_idx, ents, next_blk in self.children_blocks:
                f.seek(blk_idx * BS)
                f.write(pack_children_block(ents, next_blk))
            # file contents
            for node in self.nodes:
                if node['kind'] != OBJ_KIND_FILE:
                    continue
                start = node.get('data_start_block', 0)
                nblocks = node.get('data_num_blocks', 0)
                if nblocks == 0:
                    continue
                with open(node['path'], 'rb') as src:
                    remain = node['size']
                    for bi in range(nblocks):
                        data = src.read(min(remain, BS))
                        if data is None:
                            data = b''
                        data = data.ljust(BS, b'\x00')
                        f.seek((start + bi) * BS)
                        f.write(data)
                        remain -= min(remain, BS)
            # free map: 1 bit per block, 1=free, 0=used
            # cover [0, total_blocks)
            freebits = bytearray(self.free_map_blocks * BS)
            for b in range(total_blocks):
                byte = b // 8
                bit = b % 8
                is_free = (b not in self.used_blocks)
                if is_free:
                    freebits[byte] |= (1 << bit)
                else:
                    freebits[byte] &= ~(1 << bit)
            f.seek(self.free_map_start * BS)
            f.write(bytes(freebits))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--diskdir', required=True, help='Source directory tree')
    ap.add_argument('--out', required=True, help='Output image path')
    ap.add_argument('--size-mb', type=int, default=64, help='Image size (MB)')
    args = ap.parse_args()
    b = Builder(args.diskdir)
    b.scan()
    total_blocks = (args.size_mb * 1024 * 1024) // BS
    if total_blocks < 32:
      total_blocks = 32
    b.write(args.out, total_blocks)
    print(f'Wrote ObjectFS image: {args.out}', file=sys.stderr)

if __name__ == '__main__':
    main()



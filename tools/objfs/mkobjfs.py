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

# Attribute value kinds (match objfs_attr_type_t / objfs_attr_value_kind_t)
OBJFS_ATTR_STR = 0
OBJFS_ATTR_INT = 1
OBJFS_ATTR_BOOL = 2

def parse_attributes_yaml(path: Path):
    """
    Minimal YAML parser for simple key: value pairs.
    Supports:
      - booleans: true/false (case-insensitive)
      - integers: e.g., 123 or -10
      - strings: unquoted or quoted with '...' or "..."
    Returns list of tuples: (key, type, value)
    """
    attrs = []
    if not path.exists():
        return attrs
    try:
        with open(path, 'r', encoding='utf-8') as f:
            for raw in f:
                line = raw.strip()
                if not line or line.startswith('#'):
                    continue
                if ':' not in line:
                    continue
                k, v = line.split(':', 1)
                key = k.strip()
                val = v.strip()
                if ' #' in val:
                    val = val.split(' #', 1)[0].rstrip()
                if val.startswith('"') and val.endswith('"') and len(val) >= 2:
                    attrs.append((key, OBJFS_ATTR_STR, val[1:-1]))
                elif val.startswith("'") and val.endswith("'") and len(val) >= 2:
                    attrs.append((key, OBJFS_ATTR_STR, val[1:-1]))
                else:
                    lo = val.lower()
                    if lo == 'true':
                        attrs.append((key, OBJFS_ATTR_BOOL, True))
                    elif lo == 'false':
                        attrs.append((key, OBJFS_ATTR_BOOL, False))
                    else:
                        try:
                            attrs.append((key, OBJFS_ATTR_INT, int(val, 10)))
                        except ValueError:
                            attrs.append((key, OBJFS_ATTR_STR, val))
    except Exception:
        return []
    return attrs

def pack_super(sb):
    magic = b'OBJFS1\x00\x00'
    # Layout matches objfs_superblock_t:
    # magic[8], version(u32), block_size(u32),
    # root_object_id(Q), object_table_start(Q), object_table_blocks(Q),
    # free_map_start(Q), free_map_blocks(Q), content_start(Q),
    # _reserved64[8] (8Q)
    return struct.pack('<8sIIQQQQQQ8Q',
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
    # objfs_object_disk_t layout:
    # Q id, H mode, B kind, B flags, I uid, I gid, I nlink,
    # Q size, Q atime, Q mtime, Q ctime, Q target_id,
    # Q subobjects_idx, Q attrs_head, Q data_start_block,
    # I data_num_blocks, I _pad32, 8Q _reserved64
    return struct.pack('<QHBBIIIQQQQQQQQII8Q',
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
        o.get('subobjects_idx',0),
        o.get('attrs_head',0),
        o.get('data_start_block',0),
        o.get('data_num_blocks',0),
        0, # _pad32
        0,0,0,0,0,0,0,0 # reserved
    )

def pack_subobjects_block(entries, next_block):
    hdr = struct.pack('<IIQ', len(entries), 0, next_block)
    body = b''
    for name, kind, subobject_id in entries:
        n = name.encode('utf-8')
        n = n[:64]
        name_len = len(n)
        n_padded = n + b'\x00'*(64-len(n))
        body += struct.pack('<BBH64sQ', name_len, kind, 0, n_padded, subobject_id)
    blk = hdr + body
    if len(blk) > BS:
        raise RuntimeError('subobjects block overflow')
    return blk + b'\x00'*(BS-len(blk))

def pack_attrs_block(attrs, next_block):
    """
    attrs: list of tuples (key:str, type:int, value:any)
    Returns exactly one block (4096 bytes) containing the attrs header and entries.
    """
    # Header: count(u32), _pad(u32), next_block(u64)
    entries_blob = bytearray()
    count = 0
    for key, t, v in attrs:
        kbytes = key.encode('utf-8')[:64]
        key_len = len(kbytes)
        kpad = kbytes + b'\x00'*(64 - key_len)
        if t == OBJFS_ATTR_STR:
            vbytes = v.encode('utf-8') if isinstance(v, str) else bytes(v)
            vbytes = vbytes[:(BS - 128)]  # hard cap for safety
            vlen = len(vbytes)
            entry = struct.pack('<BBH64s', key_len, t, vlen, kpad) + vbytes
            # align to 8
            if len(entry) % 8:
                entry += b'\x00' * (8 - (len(entry) % 8))
        elif t == OBJFS_ATTR_INT:
            entry = struct.pack('<BBH64s', key_len, t, 0, kpad) + struct.pack('<q', int(v))
            if len(entry) % 8:
                entry += b'\x00' * (8 - (len(entry) % 8))
        elif t == OBJFS_ATTR_BOOL:
            entry = struct.pack('<BBH64s', key_len, t, 0, kpad) + struct.pack('<B', 1 if v else 0)
            if len(entry) % 8:
                entry += b'\x00' * (8 - (len(entry) % 8))
        else:
            raise RuntimeError('unknown attribute type')
        if len(entries_blob) + len(entry) + 16 > BS:
            raise RuntimeError('attributes block overflow')
        entries_blob += entry
        count += 1
    hdr = struct.pack('<IIQ', count, 0, next_block)
    blob = hdr + entries_blob
    if len(blob) > BS:
        raise RuntimeError('attributes block overflow post-pack')
    return blob + b'\x00' * (BS - len(blob))

class Builder:
    def __init__(self, diskdir):
        self.diskdir = Path(diskdir)
        self.nodes = []  # list of dicts {id, path, kind, size, file_path, dir_metadata_path, attrs_head, attrs[]}
        self.path_to_id = {}
        self.subobject_entries = {}  # id -> list of (name, kind, subobject_id)
        self.subobject_names = {}    # id -> set(name)
        self.objects = []  # filled later with layout
        self.subobjects_blocks = []  # (block_index, entries, next_block)
        self.content_blocks = []   # (block_index, data)
        self.attr_blocks = []      # (block_index, bytes)
        self.next_free_block = 0
        self.object_table_start = 0
        self.object_table_blocks = 0
        self.free_map_start = 0
        self.free_map_blocks = 0
        self.content_region_start = 0
        self.total_blocks = 0
        self.used_blocks = set()

    def scan(self):
        # Build logical object model with *.obj support
        root_id = 0
        self._add_node(self.diskdir, root_id, OBJ_KIND_DIR)
        self._ensure_subentries(root_id)
        self._scan_root(self.diskdir, root_id)

    def _add_node(self, path, oid, kind, size=0):
        self.nodes.append({
            'id': oid,
            'path': Path(path),
            'kind': kind,
            'size': size,
            'file_path': Path(path) if kind == OBJ_KIND_FILE else None,
            'dir_metadata_path': None,
            'attrs_head': 0,
            'attrs': [],
        })
        self.path_to_id[Path(path)] = oid

    def _ensure_subentries(self, parent_id):
        if parent_id not in self.subobject_entries:
            self.subobject_entries[parent_id] = []
        if parent_id not in self.subobject_names:
            self.subobject_names[parent_id] = set()

    def _add_subentry(self, parent_id, name, kind, child_id):
        self._ensure_subentries(parent_id)
        if name in self.subobject_names[parent_id]:
            parent_path = str(self.nodes[parent_id]['path']) if parent_id < len(self.nodes) else f'id={parent_id}'
            raise RuntimeError(f"duplicate subobject name '{name}' under {parent_path}; "
                               f"remove one of '{name}' and '{name}.obj' (or other duplicates)")
        self.subobject_names[parent_id].add(name)
        self.subobject_entries[parent_id].append((name, kind, child_id))

    def _scan_root(self, host_dir: Path, parent_id: int):
        entries = sorted([p for p in host_dir.iterdir()], key=lambda p: p.name)
        for p in entries:
            if p.is_dir() and p.name.endswith('.obj'):
                self._scan_obj_container(p, parent_id, p.name[:-4])
            elif p.is_dir():
                self._scan_regular_dir(p, parent_id, p.name)
            elif p.is_file():
                oid = len(self.nodes)
                size = p.stat().st_size
                self._add_node(p, oid, OBJ_KIND_FILE, size=size)
                self._add_subentry(parent_id, p.name, OBJ_KIND_FILE, oid)

    def _scan_regular_dir(self, dpath: Path, parent_id: int, entry_name: str):
        dir_oid = len(self.nodes)
        self._add_node(dpath, dir_oid, OBJ_KIND_DIR)
        self._add_subentry(parent_id, entry_name, OBJ_KIND_DIR, dir_oid)
        entries = sorted([p for p in dpath.iterdir()], key=lambda p: p.name)
        for p in entries:
            if p.is_dir() and p.name.endswith('.obj'):
                self._scan_obj_container(p, dir_oid, p.name[:-4])
            elif p.is_dir():
                self._scan_regular_dir(p, dir_oid, p.name)
            elif p.is_file():
                oid = len(self.nodes)
                size = p.stat().st_size
                self._add_node(p, oid, OBJ_KIND_FILE, size=size)
                self._add_subentry(dir_oid, p.name, OBJ_KIND_FILE, oid)

    def _scan_obj_container(self, objdir: Path, parent_id: int, logical_name: str):
        oid = len(self.nodes)
        self._add_node(objdir, oid, OBJ_KIND_DIR, size=0)
        self._add_subentry(parent_id, logical_name, OBJ_KIND_DIR, oid)
        # contents file
        contents_path = objdir / 'contents'
        if contents_path.exists() and contents_path.is_file():
            self.nodes[oid]['dir_metadata_path'] = contents_path
            self.nodes[oid]['size'] = contents_path.stat().st_size
        # attributes.yaml
        attrs_path = objdir / 'attributes.yaml'
        attrs = parse_attributes_yaml(attrs_path)
        if attrs:
            self.nodes[oid]['attrs'] = attrs
        # subobjects
        subs_dir = objdir / 'subobjects'
        if subs_dir.exists() and subs_dir.is_dir():
            entries = sorted([p for p in subs_dir.iterdir()], key=lambda p: p.name)
            for p in entries:
                if p.is_dir() and p.name.endswith('.obj'):
                    self._scan_obj_container(p, oid, p.name[:-4])
                elif p.is_dir():
                    self._scan_regular_dir(p, oid, p.name)
                elif p.is_file():
                    cid = len(self.nodes)
                    size = p.stat().st_size
                    self._add_node(p, cid, OBJ_KIND_FILE, size=size)
                    self._add_subentry(oid, p.name, OBJ_KIND_FILE, cid)

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
        # subobjects blocks: allocate for any node that actually has children
        for node in self.nodes:
            ents = self.subobject_entries.get(node['id'], [])
            if ents:
                ents = sorted(ents, key=lambda e: e[0])
                # one block should be enough for MVP; if not, chain single block anyway
                blk_idx = self.next_free_block
                self.next_free_block += 1
                self.subobjects_blocks.append((blk_idx, ents, 0))
                node['subobjects_idx'] = blk_idx
                self.used_blocks.add(blk_idx)
            else:
                node['subobjects_idx'] = 0
        # content blocks (file contents or directory metadata-as-contents)
        for node in self.nodes:
            size = node.get('size', 0)
            has_dir_meta = bool(node.get('dir_metadata_path'))
            needs_content = (size > 0) or has_dir_meta
            if not needs_content:
                node['data_start_block'] = 0
                node['data_num_blocks'] = 0
                continue
            nblocks = (size + BS - 1) // BS if size > 0 else 0
            if nblocks == 0:
                node['data_start_block'] = 0
                node['data_num_blocks'] = 0
                continue
            start = self.next_free_block
            node['data_start_block'] = start
            node['data_num_blocks'] = nblocks
            self.next_free_block += nblocks
            # mark data blocks as used; we will fill later
            for bi in range(nblocks):
                self.used_blocks.add(start + bi)
        # attribute blocks
        for node in self.nodes:
            attrs = node.get('attrs', [])
            if not attrs:
                continue
            blk_idx = self.next_free_block
            self.next_free_block += 1
            packed = pack_attrs_block(attrs, 0)
            self.attr_blocks.append((blk_idx, packed))
            node['attrs_head'] = blk_idx
            self.used_blocks.add(blk_idx)

        self.objects = []
        for node in self.nodes:
            # Derive mode from capabilities: 0755 if has subobjects, else 0644
            has_children = bool(self.subobject_entries.get(node['id'], []))
            o = {
                'id': node['id'],
                # Write unknown kind; consumers should derive capabilities
                'kind': OBJ_KIND_UNKNOWN,
                'size': node.get('size',0),
                'subobjects_idx': node.get('subobjects_idx', 0),
                'attrs_head': node.get('attrs_head', 0),
                'data_start_block': node.get('data_start_block', 0),
                'data_num_blocks': node.get('data_num_blocks', 0),
                'nlink': 1,
                'mode': 0o755 if has_children else 0o644,
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
            all_objs = self.objects + [ {'id': len(self.objects)+i, 'kind': OBJ_KIND_UNKNOWN, 'size': 0, 'subobjects_idx': 0, 'attrs_head': 0, 'data_start_block': 0, 'data_num_blocks': 0, 'nlink': 0, 'mode': 0} for i in range(RESERVED_OBJ_SLOTS) ]
            for i, o in enumerate(all_objs):
                if (i % per) == 0:
                    # start new block
                    f.seek(cur_block * BS)
                    off = 0
                    cur_block += 1
                f.seek((cur_block - 1) * BS + off)
                f.write(pack_object(o))
                off += (8+2+1+1+4+4+4+8+8+8+8+8+8+8+8+4+4+8*8)
            # subobjects blocks
            for blk_idx, ents, next_blk in self.subobjects_blocks:
                f.seek(blk_idx * BS)
                # Store UNKNOWN type in index; kernels/extractors derive kind
                ents_unknown = [(name, OBJ_KIND_UNKNOWN, sub_id) for (name, _k, sub_id) in ents]
                f.write(pack_subobjects_block(ents_unknown, next_blk))
            # attributes blocks
            for blk_idx, data in self.attr_blocks:
                f.seek(blk_idx * BS)
                f.write(data)
            # file contents and directory metadata contents
            for node in self.nodes:
                start = node.get('data_start_block', 0)
                nblocks = node.get('data_num_blocks', 0)
                if nblocks == 0:
                    continue
                if node.get('file_path') is not None:
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
                elif node.get('dir_metadata_path'):
                    with open(node['dir_metadata_path'], 'rb') as src:
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


#!/usr/bin/env python3
"""
ObjectFS extractor: dump an ObjectFS image into a readable host directory.

This is the inverse of mkobjfs.py. Given an ObjectFS image, it walks the
object table and subobject indices and reconstructs a tree of files and
`.obj` containers that mkobjfs.py can later re-pack.

Layout it produces:
  - Root directory (the --out path) represents object 0 (root directory).
  - For each subobject of a directory:
      * FILE objects  -> plain files: <parent>/<name>
      * DIR objects   -> containers:  <parent>/<name>.obj/
          - optional   contents         (if the dir object has data)
          - optional   attributes.yaml  (if it has attributes)
          - mandatory  subobjects/      (children live here)

This matches the conventions used by mkobjfs.Builder._scan_root and
_scan_obj_container(), so a round-trip (image -> directory -> mkobjfs)
should preserve the ObjectFS contents.
"""

import argparse
import os
import struct
import sys
from pathlib import Path


# Defaults (will be overridden by superblock.block_size where relevant)
BS_DEFAULT = 4096

# Object kinds (match objfs_obj_kind_t / mkobjfs.py)
OBJ_KIND_UNKNOWN = 0
OBJ_KIND_FILE = 1
OBJ_KIND_DIR = 2
OBJ_KIND_REFERENCE = 3

# Attribute value kinds (match OBJFS_ATTR_* in mkobjfs.py / objfs_format.h)
OBJFS_ATTR_STR = 0
OBJFS_ATTR_INT = 1
OBJFS_ATTR_BOOL = 2


SUPER_FMT = "<8sIIQQQQQQ8Q"
OBJ_FMT = "<QHBBIIIQQQQQQQQII8Q"
SUBHDR_FMT = "<IIQ"
SUBENT_FMT = "<BBH64sQ"
ATTR_HDR_FMT = "<IIQ"
ATTREH_FMT = "<BBH64s"

SUPER_SIZE = struct.calcsize(SUPER_FMT)
OBJ_SIZE = struct.calcsize(OBJ_FMT)
SUBHDR_SIZE = struct.calcsize(SUBHDR_FMT)
SUBENT_SIZE = struct.calcsize(SUBENT_FMT)
ATTR_HDR_SIZE = struct.calcsize(ATTR_HDR_FMT)
ATTREH_SIZE = struct.calcsize(ATTREH_FMT)


def read_super(f):
  f.seek(0)
  data = f.read(SUPER_SIZE)
  if len(data) != SUPER_SIZE:
    raise RuntimeError("image too small for superblock")
  (
      magic,
      version,
      block_size,
      root_object_id,
      object_table_start,
      object_table_blocks,
      free_map_start,
      free_map_blocks,
      content_start,
      *_
  ) = struct.unpack(SUPER_FMT, data)
  if not magic.startswith(b"OBJFS1"):
    raise RuntimeError("not an ObjectFS image (bad magic)")
  if block_size == 0:
    block_size = BS_DEFAULT
  return {
      "magic": magic,
      "version": version,
      "block_size": block_size,
      "root_object_id": root_object_id,
      "object_table_start": object_table_start,
      "object_table_blocks": object_table_blocks,
      "free_map_start": free_map_start,
      "free_map_blocks": free_map_blocks,
      "content_start": content_start,
  }


def read_objects(f, sb):
  """Read all object descriptors into a dict keyed by id."""
  bs = sb["block_size"]
  start = sb["object_table_start"]
  blocks = sb["object_table_blocks"]
  per = bs // OBJ_SIZE
  if per == 0:
    raise RuntimeError("object descriptor larger than block size")

  objects = {}
  for b in range(blocks):
    f.seek((start + b) * bs)
    block = f.read(bs)
    if not block:
      break
    off = 0
    for i in range(per):
      if off + OBJ_SIZE > len(block):
        break
      (
          oid,
          mode,
          kind,
          flags,
          uid,
          gid,
          nlink,
          size,
          atime,
          mtime,
          ctime,
          target_id,
          subobjects_idx,
          attrs_head,
          data_start_block,
          data_num_blocks,
          _pad32,
          *_reserved,
      ) = struct.unpack_from(OBJ_FMT, block, off)
      off += OBJ_SIZE
      # Skip unused / reserved entries indicated by nlink==0
      if nlink == 0:
        continue
      objects[oid] = {
          "id": oid,
          "mode": mode,
          "kind": kind,
          "flags": flags,
          "uid": uid,
          "gid": gid,
          "nlink": nlink,
          "size": size,
          "atime": atime,
          "mtime": mtime,
          "ctime": ctime,
          "target_id": target_id,
          "subobjects_idx": subobjects_idx,
          "attrs_head": attrs_head,
          "data_start_block": data_start_block,
          "data_num_blocks": data_num_blocks,
      }
  return objects


def read_subobjects(f, sb, objects):
  """Build parent_id -> list of {name, kind, id} from subobject blocks.
  Kind in entries is treated as a hint; we will derive actual kind later."""
  bs = sb["block_size"]
  children = {oid: [] for oid in objects.keys()}
  for oid, obj in objects.items():
    idx = obj.get("subobjects_idx", 0)
    if not idx:
      continue
    block_idx = idx
    while block_idx:
      f.seek(block_idx * bs)
      buf = f.read(bs)
      if len(buf) < SUBHDR_SIZE:
        break
      count, _pad, next_block = struct.unpack_from(SUBHDR_FMT, buf, 0)
      off = SUBHDR_SIZE
      for _ in range(count):
        if off + SUBENT_SIZE > len(buf):
          break
        name_len, kind, _pad16, name_bytes, sub_id = struct.unpack_from(
            SUBENT_FMT, buf, off
        )
        off += SUBENT_SIZE
        name = name_bytes[:name_len].decode("utf-8", errors="replace")
        children.setdefault(oid, []).append(
            {"name": name, "kind": kind, "id": sub_id}
        )
      block_idx = next_block
  return children


def read_data(f, sb, obj):
  """Read object data (file contents or directory metadata) as bytes."""
  size = obj.get("size", 0)
  nblocks = obj.get("data_num_blocks", 0)
  start = obj.get("data_start_block", 0)
  if size == 0 or nblocks == 0 or start == 0:
    return b""
  bs = sb["block_size"]
  remaining = size
  chunks = []
  for i in range(nblocks):
    if remaining <= 0:
      break
    f.seek((start + i) * bs)
    block = f.read(bs)
    if not block:
      break
    take = min(remaining, len(block))
    chunks.append(block[:take])
    remaining -= take
  return b"".join(chunks)


def read_attrs(f, sb, obj):
  """Read attribute entries for an object, returning list of (key, type, value)."""
  head = obj.get("attrs_head", 0)
  if not head:
    return []
  bs = sb["block_size"]
  attrs = []
  block_idx = head
  while block_idx:
    f.seek(block_idx * bs)
    buf = f.read(bs)
    if len(buf) < ATTR_HDR_SIZE:
      break
    count, _pad, next_block = struct.unpack_from(ATTR_HDR_FMT, buf, 0)
    off = ATTR_HDR_SIZE
    for _ in range(count):
      if off + ATTREH_SIZE > len(buf):
        break
      key_len, atype, vlen, key_bytes = struct.unpack_from(ATTREH_FMT, buf, off)
      off += ATTREH_SIZE
      key = key_bytes[:key_len].decode("utf-8", errors="replace")
      if atype == OBJFS_ATTR_STR:
        # vlen bytes of string, then 8-byte alignment
        if off + vlen > len(buf):
          break
        raw = buf[off : off + vlen]
        off += vlen
        if (ATTREH_SIZE + vlen) % 8:
          pad = 8 - ((ATTREH_SIZE + vlen) % 8)
          off += pad
        value = raw.decode("utf-8", errors="replace")
      elif atype == OBJFS_ATTR_INT:
        # int64_t, then 8-byte alignment
        if off + 8 > len(buf):
          break
        (ival,) = struct.unpack_from("<q", buf, off)
        off += 8
        if (ATTREH_SIZE + 8) % 8:
          pad = 8 - ((ATTREH_SIZE + 8) % 8)
          off += pad
        value = ival
      elif atype == OBJFS_ATTR_BOOL:
        # uint8_t, then 8-byte alignment
        if off + 1 > len(buf):
          break
        bval = buf[off]
        off += 1
        if (ATTREH_SIZE + 1) % 8:
          pad = 8 - ((ATTREH_SIZE + 1) % 8)
          off += pad
        value = bool(bval)
      else:
        # Unknown type; stop parsing this block to avoid confusion
        break
      attrs.append((key, atype, value))
    block_idx = next_block
  return attrs


def _normalize_path_component(name: str) -> str:
  """
  Ensure ObjectFS entry names stay as single safe path components.
  Leading slashes are stripped (so `/foo` maps to `foo`), path traversal
  segments are rejected, and nested paths like `foo/bar` are forbidden.
  """
  if not name:
    raise RuntimeError("empty object name")
  sanitized = name.replace("\\", "/").lstrip("/")
  parts = []
  for part in sanitized.split("/"):
    if part in ("", "."):
      continue
    if part == "..":
      raise RuntimeError(f"illegal parent reference in name: {name!r}")
    parts.append(part)
  if not parts:
    raise RuntimeError(f"name collapses to empty after sanitization: {name!r}")
  if len(parts) > 1:
    raise RuntimeError(f"nested path components not allowed in name: {name!r}")
  return parts[0]


def write_attrs_yaml(path: Path, attrs):
  """Emit attributes.yaml matching parse_attributes_yaml() expectations."""
  lines = []
  for key, atype, value in attrs:
    if atype == OBJFS_ATTR_STR:
      sval = str(value)
      # simple quoting; mkobjfs parser understands quoted strings
      sval = sval.replace('"', '\\"')
      lines.append(f'{key}: "{sval}"')
    elif atype == OBJFS_ATTR_INT:
      lines.append(f"{key}: {int(value)}")
    elif atype == OBJFS_ATTR_BOOL:
      lines.append(f"{key}: {'true' if value else 'false'}")
  if not lines:
    return
  path.parent.mkdir(parents=True, exist_ok=True)
  with open(path, "w", encoding="utf-8") as f:
    for line in lines:
      f.write(line)
      f.write("\n")


def extract_dir(f, sb, objects, children, obj_id, host_dir: Path, is_root: bool):
  """Recursively extract directory object obj_id into host_dir."""
  host_dir.mkdir(parents=True, exist_ok=True)
  entries = children.get(obj_id, [])
  for ent in entries:
    raw_name = ent["name"]
    try:
      name = _normalize_path_component(raw_name)
    except RuntimeError as exc:
      print(f"warning: skipping {raw_name!r}: {exc}", file=sys.stderr)
      continue
    child_id = ent["id"]
    child = objects.get(child_id)
    if child is None:
      continue
    # Derive capability-based kind for extraction
    kind = OBJ_KIND_UNKNOWN
    if child.get("target_id", 0) != 0:
      kind = OBJ_KIND_REFERENCE
    elif child.get("subobjects_idx", 0) != 0:
      kind = OBJ_KIND_DIR
    elif child.get("data_num_blocks", 0) != 0 or child.get("size", 0) != 0:
      kind = OBJ_KIND_FILE

    if kind == OBJ_KIND_FILE:
      # Plain file: <parent>/<name>
      out_path = host_dir / name
      out_path.parent.mkdir(parents=True, exist_ok=True)
      data = read_data(f, sb, child)
      with open(out_path, "wb") as out_f:
        out_f.write(data)

    elif kind == OBJ_KIND_DIR:
      # Represent directory objects as <name>.obj containers.
      container = host_dir / (name + ".obj")
      container.mkdir(parents=True, exist_ok=True)

      # Optional "contents" file for directory metadata (if the dir has data).
      data = read_data(f, sb, child)
      if data:
        with open(container / "contents", "wb") as cf:
          cf.write(data)

      # Optional attributes.yaml
      attrs = read_attrs(f, sb, child)
      if attrs:
        write_attrs_yaml(container / "attributes.yaml", attrs)

      # Recurse into subobjects/
      subdir = container / "subobjects"
      extract_dir(f, sb, objects, children, child_id, subdir, is_root=False)

    elif kind == OBJ_KIND_REFERENCE:
      # Simple textual representation for references.
      out_path = host_dir / (name + ".ref")
      out_path.parent.mkdir(parents=True, exist_ok=True)
      with open(out_path, "w", encoding="utf-8") as rf:
        rf.write(f"target_id: {child.get('target_id', 0)}\n")
    else:
      # Unknown kind; skip.
      continue


def main():
  ap = argparse.ArgumentParser(description="Extract an ObjectFS image to a directory")
  ap.add_argument("--img", required=True, help="Path to ObjectFS image (e.g. data.img)")
  ap.add_argument(
      "--out",
      required=True,
      help="Output directory to populate with extracted tree",
  )
  ap.add_argument(
      "--force",
      action="store_true",
      help="Allow extracting into a non-empty directory",
  )
  args = ap.parse_args()

  out_dir = Path(args.out)
  if out_dir.exists():
    if not args.force:
      # Refuse to clobber non-empty directory unless --force
      if any(out_dir.iterdir()):
        print(
            f"error: output directory {out_dir} is not empty (use --force to override)",
            file=sys.stderr,
        )
        sys.exit(1)
  else:
    out_dir.mkdir(parents=True, exist_ok=True)

  img_path = Path(args.img)
  with open(img_path, "rb") as f:
    sb = read_super(f)
    objects = read_objects(f, sb)
    root_id = sb["root_object_id"]
    if root_id not in objects:
      raise RuntimeError(f"root object id {root_id} not found in object table")
    children = read_subobjects(f, sb, objects)
    # Root is represented directly by out_dir; its children become top-level
    extract_dir(f, sb, objects, children, root_id, out_dir, is_root=True)

  print(f"Extracted ObjectFS image {img_path} to {out_dir}", file=sys.stderr)


if __name__ == "__main__":
  main()



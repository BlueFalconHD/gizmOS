#include "gzfs.h"
#include <lib/memory.h>

static g_bool gzfs_read_inode(gzfs_fs_t *fs, uint32_t inum, gzfs_inode_disk_t *out) {
  if (inum == 0 || inum > fs->sb.inode_count) return false;
  uint64_t inodes_per_block = BCACHE_BLOCK_SIZE / sizeof(gzfs_inode_disk_t);
  uint64_t idx = inum - 1;
  uint64_t blk = fs->sb.inode_start + (idx / inodes_per_block);
  uint64_t off = (idx % inodes_per_block) * sizeof(gzfs_inode_disk_t);
  bcache_buf_t *b = bcache_get(fs->bc, blk);
  if (!b) return false;
  memcpy(out, b->data + off, sizeof(gzfs_inode_disk_t));
  bcache_put(fs->bc, b);
  return true;
}

static g_bool gzfs_block_map(gzfs_fs_t *fs, const gzfs_inode_disk_t *din,
                             uint32_t file_block_index, uint32_t *out_phys_block) {
  if (file_block_index < 12) {
    *out_phys_block = din->direct[file_block_index];
    return true;
  }
  file_block_index -= 12;
  /* single indirect */
  if (file_block_index < (BCACHE_BLOCK_SIZE / sizeof(uint32_t))) {
    uint32_t iblk = din->indirect1;
    if (iblk == 0) { *out_phys_block = 0; return true; }
    bcache_buf_t *b = bcache_get(fs->bc, fs->sb.data_start + iblk);
    if (!b) return false;
    uint32_t *arr = (uint32_t *)b->data;
    *out_phys_block = arr[file_block_index];
    bcache_put(fs->bc, b);
    return true;
  }
  return false; /* no double indirect yet */
}

/* Public helpers used by ops */
g_bool gzfs_load_vnode(vfs_node_t *vn, gzfs_inode_t *out) {
  gzfs_fs_t *fs = (gzfs_fs_t *)vn->mnt->fs_private;
  out->fs = fs;
  out->inum = vn->inum;
  return gzfs_read_inode(fs, vn->inum, &out->din);
}

g_bool gzfs_read_at(vfs_node_t *vn, uint64_t off, void *buf, size_t n, size_t *outn) {
  /* If vnode is a fork file, route to fork entry-based read */
  if (vn->fs_private) {
    gzfs_fork_priv_t *fp = (gzfs_fork_priv_t *)vn->fs_private;
    if (fp->kind == GZFS_PRIV_FORKFILE) {
      gzfs_fs_t *fs = (gzfs_fs_t *)vn->mnt->fs_private;
      const gzfs_fork_entry_disk_t *fe = &fp->entry;
      if (off >= fe->size) { if (outn) *outn = 0; return true; }
      if (off + n > fe->size) n = (size_t)(fe->size - off);

      size_t done_f = 0;
      while (done_f < n) {
        uint64_t fblk = (off + done_f) / BCACHE_BLOCK_SIZE;
        uint64_t blk_off = (off + done_f) % BCACHE_BLOCK_SIZE;
        uint32_t pblk = 0;
        if (fblk < 12) {
          pblk = fe->direct[fblk];
        } else {
          uint32_t idx = (uint32_t)(fblk - 12);
          if (fe->indirect1 == 0) break;
          bcache_buf_t *ib = bcache_get(fs->bc, fs->sb.data_start + fe->indirect1);
          if (!ib) return false;
          uint32_t *arr = (uint32_t *)ib->data;
          pblk = arr[idx];
          bcache_put(fs->bc, ib);
        }
        if (pblk == 0) break;
        uint64_t phys = fs->sb.data_start + pblk;
        bcache_buf_t *b = bcache_get(fs->bc, phys);
        if (!b) return false;
        size_t take = n - done_f;
        size_t avail = BCACHE_BLOCK_SIZE - blk_off;
        if (take > avail) take = avail;
        memcpy((uint8_t *)buf + done_f, b->data + blk_off, take);
        bcache_put(fs->bc, b);
        done_f += take;
      }
      if (outn) *outn = done_f;
      return true;
    }
  }

  gzfs_inode_t in;
  if (!gzfs_load_vnode(vn, &in)) return false;
  if (off >= in.din.size) { if (outn) *outn = 0; return true; }
  if (off + n > in.din.size) n = (size_t)(in.din.size - off);

  size_t done = 0;
  while (done < n) {
    uint64_t fblk = (off + done) / BCACHE_BLOCK_SIZE;
    uint64_t blk_off = (off + done) % BCACHE_BLOCK_SIZE;
    uint32_t pblk = 0;
    if (!gzfs_block_map(in.fs, &in.din, (uint32_t)fblk, &pblk)) return false;
    if (pblk == 0) break;
    uint64_t phys = in.fs->sb.data_start + pblk;
    bcache_buf_t *b = bcache_get(in.fs->bc, phys);
    if (!b) return false;
    size_t take = n - done;
    size_t avail = BCACHE_BLOCK_SIZE - blk_off;
    if (take > avail) take = avail;
    memcpy((uint8_t *)buf + done, b->data + blk_off, take);
    bcache_put(in.fs->bc, b);
    done += take;
  }
  if (outn) *outn = done;
  return true;
}

g_bool gzfs_getattr_vnode(vfs_node_t *vn, vfs_stat_t *st) {
  /* If fork synthetic nodes, report from fork metadata */
  if (vn->fs_private) {
    gzfs_fork_priv_t *fp = (gzfs_fork_priv_t *)vn->fs_private;
    if (fp->kind == GZFS_PRIV_FORKFILE) {
      st->size = fp->entry.size;
      st->mode = 0644;
      st->type = VFS_NODE_FILE;
      st->nlink = 1;
      return true;
    } else if (fp->kind == GZFS_PRIV_FORKDIR) {
      st->size = 0;
      st->mode = 0555;
      st->type = VFS_NODE_DIR;
      st->nlink = 1;
      return true;
    }
  }

  gzfs_inode_t in;
  if (!gzfs_load_vnode(vn, &in)) return false;
  st->size = in.din.size;
  st->mode = in.din.mode;
  st->type = in.din.type;
  st->nlink = in.din.nlink;
  return true;
}



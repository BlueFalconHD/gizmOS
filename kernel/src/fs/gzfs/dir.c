#include "gzfs.h"
#include <lib/kalloc.h>
#include <lib/memory.h>
#include <lib/str.h>

typedef struct __attribute__((packed)) {
  uint32_t inum;
  uint8_t  type;    /* vfs_node_kind_t */
  uint8_t  namelen;
  uint16_t _pad;
  char name[64];
} gzfs_dirent_t;

static g_bool dir_lookup_one(gzfs_fs_t *fs, const gzfs_inode_disk_t *din,
                             const char *name, uint32_t *out_inum, uint8_t *out_type) {
  uint64_t off = 0;
  while (off < din->size) {
    uint64_t fblk = off / BCACHE_BLOCK_SIZE;
    uint64_t blk_off = off % BCACHE_BLOCK_SIZE;
    uint32_t pblk = 0;
    if (!din) return false;
    /* map */
    if (fblk < 12) pblk = din->direct[fblk];
    else {
      uint32_t idx = (uint32_t)(fblk - 12);
      if (din->indirect1 == 0) return false;
      bcache_buf_t *ib = bcache_get(fs->bc, fs->sb.data_start + din->indirect1);
      if (!ib) return false;
      uint32_t *arr = (uint32_t *)ib->data;
      pblk = arr[idx];
      bcache_put(fs->bc, ib);
    }
    if (pblk == 0) return false;
    bcache_buf_t *b = bcache_get(fs->bc, fs->sb.data_start + pblk);
    if (!b) return false;
    size_t avail = BCACHE_BLOCK_SIZE - blk_off;
    size_t left = (size_t)(din->size - off);
    size_t to_scan = (left < avail) ? left : avail;

    size_t pos = 0;
    while (pos + sizeof(gzfs_dirent_t) <= to_scan) {
      gzfs_dirent_t *de = (gzfs_dirent_t *)(b->data + blk_off + pos);
      if (de->inum != 0 && de->namelen > 0 && de->namelen <= 64) {
        char nm[65];
        size_t n = de->namelen;
        if (n > 64) n = 64;
        memcpy(nm, de->name, n);
        nm[n] = '\0';
        if (strcmp(nm, name)) {
          *out_inum = de->inum;
          if (out_type) *out_type = de->type;
          bcache_put(fs->bc, b);
          return true;
        }
      }
      pos += sizeof(gzfs_dirent_t);
    }
    bcache_put(fs->bc, b);
    off += to_scan;
  }
  return false;
}

/* VFS ops glue */
static result_t gzfs_lookup(vfs_mount_t *mnt, vfs_node_t *dir, const char *name, vfs_node_t **out) {
  gzfs_fs_t *fs = (gzfs_fs_t *)mnt->fs_private;
  /* If parent is a synthetic forkdir, resolve fork files */
  if (dir->fs_private) {
    gzfs_forkdir_priv_t *fp = (gzfs_forkdir_priv_t *)dir->fs_private;
    if (fp->kind == GZFS_PRIV_FORKDIR) {
      /* Load parent inode's fork table and find by name */
      uint64_t ipb = BCACHE_BLOCK_SIZE / sizeof(gzfs_inode_disk_t);
      uint64_t idxp = fp->parent_inum - 1;
      uint64_t iblkp = fs->sb.inode_start + (idxp / ipb);
      uint64_t ioffp = (idxp % ipb) * sizeof(gzfs_inode_disk_t);
      bcache_buf_t *ibp = bcache_get(fs->bc, iblkp);
      if (!ibp) return RESULT_FAILURE(RESULT_ERROR);
      gzfs_inode_disk_t dinp;
      memcpy(&dinp, ibp->data + ioffp, sizeof(dinp));
      bcache_put(fs->bc, ibp);
      uint32_t fork_tbl_rel = dinp.reserved[0];
      if (fork_tbl_rel == 0) return RESULT_FAILURE(RESULT_NOENT);
      uint64_t fork_tbl_blk = fs->sb.data_start + fork_tbl_rel;
      bcache_buf_t *tb = bcache_get(fs->bc, fork_tbl_blk);
      if (!tb) return RESULT_FAILURE(RESULT_ERROR);
      gzfs_fork_table_header_disk_t hdr;
      memcpy(&hdr, tb->data, sizeof(hdr));
      size_t pos = sizeof(hdr);
      while (pos + sizeof(gzfs_fork_entry_disk_t) <= BCACHE_BLOCK_SIZE) {
        gzfs_fork_entry_disk_t *fe = (gzfs_fork_entry_disk_t *)(tb->data + pos);
        char nm[65];
        size_t n = fe->namelen;
        if (n > 64) n = 64;
        memcpy(nm, fe->name, n);
        nm[n] = '\0';
        if (strcmp(nm, name)) {
          vfs_node_t *vn = (vfs_node_t *)kalloc(sizeof(vfs_node_t));
          if (!vn) { bcache_put(fs->bc, tb); return RESULT_FAILURE(RESULT_NOMEM); }
          memset(vn, 0, sizeof(*vn));
          vn->mnt = mnt;
          vn->inum = fp->parent_inum; /* anchored to parent inode */
          vn->type = VFS_NODE_FILE;
          vn->mode = 0644;
          gzfs_fork_priv_t *priv = (gzfs_fork_priv_t *)kalloc(sizeof(gzfs_fork_priv_t));
          if (!priv) { kfree(vn); bcache_put(fs->bc, tb); return RESULT_FAILURE(RESULT_NOMEM); }
          priv->kind = GZFS_PRIV_FORKFILE;
          priv->parent_inum = fp->parent_inum;
          memcpy(&priv->entry, fe, sizeof(*fe));
          vn->fs_private = priv;
          *out = vn;
          bcache_put(fs->bc, tb);
          return RESULT_SUCCESS(0);
        }
        pos += sizeof(gzfs_fork_entry_disk_t);
      }
      bcache_put(fs->bc, tb);
      return RESULT_FAILURE(RESULT_NOENT);
    }
  }
  gzfs_inode_disk_t din;
  /* load directory inode */
  uint64_t inodes_per_block = BCACHE_BLOCK_SIZE / sizeof(gzfs_inode_disk_t);
  uint64_t idx = dir->inum - 1;
  uint64_t blk = fs->sb.inode_start + (idx / inodes_per_block);
  uint64_t off = (idx % inodes_per_block) * sizeof(gzfs_inode_disk_t);
  bcache_buf_t *ib = bcache_get(fs->bc, blk);
  if (!ib) return RESULT_FAILURE(RESULT_ERROR);
  memcpy(&din, ib->data + off, sizeof(din));
  bcache_put(fs->bc, ib);

  /* Special synthetic child: ..namedfork directory for any non-dir */
  if (strcmp(name, "..namedfork")) {
    vfs_node_t *vn = (vfs_node_t *)kalloc(sizeof(vfs_node_t));
    if (!vn) return RESULT_FAILURE(RESULT_NOMEM);
    memset(vn, 0, sizeof(*vn));
    vn->mnt = mnt;
    vn->inum = dir->inum; /* same inum; distinguished by fs_private */
    vn->type = VFS_NODE_DIR;
    vn->mode = 0555;
    gzfs_forkdir_priv_t *priv = (gzfs_forkdir_priv_t *)kalloc(sizeof(gzfs_forkdir_priv_t));
    if (!priv) { kfree(vn); return RESULT_FAILURE(RESULT_NOMEM); }
    priv->kind = GZFS_PRIV_FORKDIR;
    priv->parent_inum = dir->inum;
    vn->fs_private = priv;
    *out = vn;
    return RESULT_SUCCESS(0);
  }

  uint32_t inum = 0;
  uint8_t type = 0;
  if (!dir_lookup_one(fs, &din, name, &inum, &type))
    return RESULT_FAILURE(RESULT_NOENT);

  vfs_node_t *vn = (vfs_node_t *)kalloc(sizeof(vfs_node_t));
  if (!vn) return RESULT_FAILURE(RESULT_NOMEM);
  memset(vn, 0, sizeof(*vn));
  vn->mnt = mnt;
  vn->inum = inum;
  vn->type = type;
  vn->mode = 0644;
  *out = vn;
  return RESULT_SUCCESS(0);
}

static result_t gzfs_getattr(vfs_mount_t *mnt, vfs_node_t *node, vfs_stat_t *st) {
  gzfs_fs_t *fs = (gzfs_fs_t *)mnt->fs_private;
  uint64_t ipb = BCACHE_BLOCK_SIZE / sizeof(gzfs_inode_disk_t);
  uint64_t idx = node->inum - 1;
  uint64_t blk = fs->sb.inode_start + (idx / ipb);
  uint64_t off = (idx % ipb) * sizeof(gzfs_inode_disk_t);
  bcache_buf_t *ib = bcache_get(fs->bc, blk);
  if (!ib) return RESULT_FAILURE(RESULT_ERROR);
  gzfs_inode_disk_t din;
  memcpy(&din, ib->data + off, sizeof(din));
  bcache_put(fs->bc, ib);
  st->size = din.size;
  st->mode = din.mode;
  st->type = din.type;
  st->nlink = din.nlink;
  return RESULT_SUCCESS(0);
}

extern g_bool gzfs_read_at(vfs_node_t *vn, uint64_t off, void *buf, size_t n, size_t *outn);

static result_t gzfs_read(vfs_mount_t *mnt, vfs_node_t *node, uint64_t off, void *buf, size_t n, size_t *outn) {
  (void)mnt;
  if (!gzfs_read_at(node, off, buf, n, outn)) return RESULT_FAILURE(RESULT_ERROR);
  return RESULT_SUCCESS(0);
}

static result_t gzfs_readdir(vfs_mount_t *mnt, vfs_node_t *dir,
                             void (*emit)(const char *name, uint32_t type, void *arg),
                             void *arg) {
  gzfs_fs_t *fs = (gzfs_fs_t *)mnt->fs_private;
  /* If this is a synthetic forkdir, enumerate fork entries from table */
  if (dir->fs_private) {
    gzfs_forkdir_priv_t *fp = (gzfs_forkdir_priv_t *)dir->fs_private;
    if (fp->kind == GZFS_PRIV_FORKDIR) {
      /* load parent inode */
      uint64_t ipb = BCACHE_BLOCK_SIZE / sizeof(gzfs_inode_disk_t);
      uint64_t idxp = fp->parent_inum - 1;
      uint64_t iblkp = fs->sb.inode_start + (idxp / ipb);
      uint64_t ioffp = (idxp % ipb) * sizeof(gzfs_inode_disk_t);
      bcache_buf_t *ibp = bcache_get(fs->bc, iblkp);
      if (!ibp) return RESULT_FAILURE(RESULT_ERROR);
      gzfs_inode_disk_t dinp;
      memcpy(&dinp, ibp->data + ioffp, sizeof(dinp));
      bcache_put(fs->bc, ibp);
      uint32_t fork_tbl_rel = dinp.reserved[0];
      if (fork_tbl_rel == 0) return RESULT_SUCCESS(0);
      uint64_t fork_tbl_blk = fs->sb.data_start + fork_tbl_rel;
      bcache_buf_t *tb = bcache_get(fs->bc, fork_tbl_blk);
      if (!tb) return RESULT_FAILURE(RESULT_ERROR);
      gzfs_fork_table_header_disk_t hdr;
      memcpy(&hdr, tb->data, sizeof(hdr));
      size_t pos = sizeof(hdr);
      for (uint32_t i = 0; i < hdr.count; i++) {
        if (pos + sizeof(gzfs_fork_entry_disk_t) > BCACHE_BLOCK_SIZE) break;
        gzfs_fork_entry_disk_t *fe = (gzfs_fork_entry_disk_t *)(tb->data + pos);
        char nm[65];
        size_t n = fe->namelen;
        if (n > 64) n = 64;
        memcpy(nm, fe->name, n);
        nm[n] = '\0';
        emit(nm, VFS_NODE_FILE, arg);
        pos += sizeof(gzfs_fork_entry_disk_t);
      }
      bcache_put(fs->bc, tb);
      return RESULT_SUCCESS(0);
    }
  }
  /* load dir inode */
  uint64_t ipb = BCACHE_BLOCK_SIZE / sizeof(gzfs_inode_disk_t);
  uint64_t idx = dir->inum - 1;
  uint64_t iblk = fs->sb.inode_start + (idx / ipb);
  uint64_t ioff = (idx % ipb) * sizeof(gzfs_inode_disk_t);
  bcache_buf_t *ib = bcache_get(fs->bc, iblk);
  if (!ib) return RESULT_FAILURE(RESULT_ERROR);
  gzfs_inode_disk_t din;
  memcpy(&din, ib->data + ioff, sizeof(din));
  bcache_put(fs->bc, ib);

  uint64_t off = 0;
  while (off < din.size) {
    uint64_t fblk = off / BCACHE_BLOCK_SIZE;
    uint64_t blk_off = off % BCACHE_BLOCK_SIZE;
    uint32_t pblk = 0;
    if (fblk < 12) pblk = din.direct[fblk];
    else {
      uint32_t idxi = (uint32_t)(fblk - 12);
      if (din.indirect1 == 0) break;
      bcache_buf_t *sib = bcache_get(fs->bc, fs->sb.data_start + din.indirect1);
      if (!sib) break;
      uint32_t *arr = (uint32_t *)sib->data;
      pblk = arr[idxi];
      bcache_put(fs->bc, sib);
    }
    if (pblk == 0) break;
    bcache_buf_t *b = bcache_get(fs->bc, fs->sb.data_start + pblk);
    if (!b) break;
    size_t avail = BCACHE_BLOCK_SIZE - blk_off;
    size_t left = (size_t)(din.size - off);
    size_t to_scan = (left < avail) ? left : avail;
    size_t pos = 0;
    while (pos + sizeof(gzfs_dirent_t) <= to_scan) {
      gzfs_dirent_t *de = (gzfs_dirent_t *)(b->data + blk_off + pos);
      if (de->inum != 0 && de->namelen > 0 && de->namelen <= 64) {
        char nm[65];
        size_t n = de->namelen;
        if (n > 64) n = 64;
        memcpy(nm, de->name, n);
        nm[n] = '\0';
        emit(nm, de->type, arg);
      }
      pos += sizeof(gzfs_dirent_t);
    }
    bcache_put(fs->bc, b);
    off += to_scan;
  }
  /* Also emit synthetic ..namedfork */
  emit("..namedfork", VFS_NODE_DIR, arg);
  return RESULT_SUCCESS(0);
}

const vfs_ops_t gzfs_ops = {
  .lookup = gzfs_lookup,
  .getattr = gzfs_getattr,
  .read = gzfs_read,
  .readdir = gzfs_readdir,
  .create = NULL,
  .mkdir = NULL,
  .link = NULL,
  .unlink = NULL,
  .symlink = NULL,
  .rename = NULL,
  .setattr = NULL,
};



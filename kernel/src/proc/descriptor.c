#include "descriptor.h"
#include "process.h"
#include <lib/kalloc.h>

int desc_alloc(struct proc *p, descriptor_t **out, int *index) {
  proc_t *owner = proc_group((proc_t *)p);
  if (!owner) return -1;
  for (int i = 0; i < PROC_MAX_DESC; i++) {
    if (owner->desc_table[i] == NULL) {
      descriptor_t *d = (descriptor_t *)kalloc(sizeof(descriptor_t));
      if (!d) return -1;
      // clear minimal fields
      d->type = 0;
      d->rights = 0;
      d->generation = 0;
      d->flags = 0;
      d->object_id = 0;
      d->offset = 0;
      owner->desc_table[i] = d;
      if (out) *out = d;
      if (index) *index = i;
      return i;
    }
  }
  return -1;
}

void desc_free(struct proc *p, int index) {
  proc_t *owner = proc_group((proc_t *)p);
  if (!owner || index < 0 || index >= PROC_MAX_DESC) return;
  descriptor_t *d = owner->desc_table[index];
  if (!d) return;
  kfree(d);
  owner->desc_table[index] = NULL;
}

int desc_get(struct proc *p, int index, descriptor_t **out) {
  proc_t *owner = proc_group((proc_t *)p);
  if (!owner || index < 0 || index >= PROC_MAX_DESC) return -1;
  descriptor_t *d = owner->desc_table[index];
  if (!d) return -1;
  if (out) *out = d;
  return 0;
}


# ObjectFS unified handle model (target state)

- Userspace uses capability-style handles for all IO and metadata.
- IDs remain for identity (`obj_id_at`, `obj_desc`, `obj_get_id(handle)`).
- Kernel provides a unified descriptor table per process (`descriptor_t`), with:
  - `type`: device, dir, file
  - `rights`: READ/WRITE/ENUM/ATTR/LINK
  - `flags`, `object_id`, `offset`, `generation`
- Syscalls return negative errors in `a0` on failure, no mixed channels.
- Userspace wrappers (`userland/libc-lite/obj.h`) expose fd-like API across objects/devices.

Migration notes:
- Prefer handle-based ops (`objh_*`); ID-based data-plane calls are legacy.
- `hello` and `shell` updated to wrappers; add more apps as needed.


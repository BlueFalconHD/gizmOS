import lldb
import shlex

# Cached HHDM offset; can be overridden with `mmu-set-hhdm`.
_MMU_HHDM_OFF = None

def _append(result, s):
    if s:
        result.AppendMessage(s)

def _err(result, s):
    result.SetError(s)

def _read_reg(exe_ctx, name):
    frame = exe_ctx.frame if exe_ctx and exe_ctx.frame and exe_ctx.frame.IsValid() else None
    if frame:
        reg = frame.FindRegister(name)
        if reg and reg.IsValid():
            return reg.GetValueAsUnsigned()
    thread = exe_ctx.thread if exe_ctx and exe_ctx.thread and exe_ctx.thread.IsValid() else None
    if thread:
        f = thread.GetSelectedFrame()
        if f and f.IsValid():
            reg = f.FindRegister(name)
            if reg and reg.IsValid():
                return reg.GetValueAsUnsigned()
    return None

def _get_hhdm_offset(target, process):
    global _MMU_HHDM_OFF
    if _MMU_HHDM_OFF is not None:
        return _MMU_HHDM_OFF, True
    # Try read the kernel's global symbol `hhdm_offset`.
    if target and target.IsValid():
        val = target.FindFirstGlobalVariable("hhdm_offset")
        if val and val.IsValid():
            try:
                off = val.GetValueAsUnsigned()
                _MMU_HHDM_OFF = off
                return off, True
            except Exception:
                pass
    return None, False

def _set_hhdm_offset(offset):
    global _MMU_HHDM_OFF
    _MMU_HHDM_OFF = offset

def _satp_decode(satp, override_sv=None):
    """
    Decode satp into (mode_name, levels, asid, root_ppn, mode_num).
    If override_sv is provided (39/48/57), it wins over satp.MODE.
    """
    mode_num = (satp >> 60) & 0xF
    asid = (satp >> 44) & 0xFFFF
    root_ppn = satp & ((1 << 44) - 1)  # PPN width on most systems
    if override_sv in (39, 48, 57):
        if override_sv == 39:
            mode_name, levels, mode_num = "SV39", 3, 8
        elif override_sv == 48:
            mode_name, levels, mode_num = "SV48", 4, 9
        else:
            mode_name, levels, mode_num = "SV57", 5, 10
    else:
        if mode_num == 8:
            mode_name, levels = "SV39", 3
        elif mode_num == 9:
            mode_name, levels = "SV48", 4
        elif mode_num == 10:
            mode_name, levels = "SV57", 5
        elif mode_num == 0:
            mode_name, levels = "Bare", 0
        else:
            mode_name, levels = f"Unknown({mode_num})", 0
    return mode_name, levels, asid, root_ppn, mode_num

def _vpn_indices(va, levels):
    """
    Return list of indices from top level down to level 0.
    For e.g. SV39 levels=3 -> [VPN[2], VPN[1], VPN[0]].
    """
    idx = []
    for lvl in reversed(range(levels)):  # levels-1 ... 0
        shift = 12 + 9 * lvl
        idx.append((va >> shift) & 0x1FF)
    return idx

def _pte_flags_str(pte):
    flags = []
    if pte & 0x1: flags.append("V")
    if pte & 0x2: flags.append("R")
    if pte & 0x4: flags.append("W")
    if pte & 0x8: flags.append("X")
    if pte & 0x10: flags.append("U")
    if pte & 0x20: flags.append("G")
    if pte & 0x40: flags.append("A")
    if pte & 0x80: flags.append("D")
    return "".join(flags) if flags else "-"

def _pte_is_leaf(pte):
    return (pte & 0x2) != 0 or (pte & 0x8) != 0  # R or X set

def _pte_is_valid(pte):
    v = (pte & 0x1) != 0
    w = (pte & 0x4) != 0
    r = (pte & 0x2) != 0
    return v and not (w and not r)

def _pte_ppn(pte):
    return (pte >> 10) & ((1 << 44) - 1)

def _read_u64(process, va):
    err = lldb.SBError() # pyright: ignore
    val = process.ReadUnsignedFromMemory(va, 8, err)
    if err.Fail():
        raise RuntimeError(err.GetCString() or "memory read failure")
    return val

def _format_addr(x):
    return f"0x{x:016x}"

def _walk_impl(target, process, satp_val, va, hhdm_off, override_sv=None, verbose=False):
    mode_name, levels, asid, root_ppn, mode_num = _satp_decode(satp_val, override_sv)
    if levels == 0:
        return {
            "ok": False,
            "reason": f"Paging mode is {mode_name} (mode={mode_num}); cannot walk."
        }

    root_pa = root_ppn << 12
    root_va = (root_pa + hhdm_off) if hhdm_off is not None else None

    path = {
        "satp": satp_val,
        "mode": mode_name,
        "levels": levels,
        "asid": asid,
        "root_ppn": root_ppn,
        "root_pa": root_pa,
        "root_va": root_va,
        "steps": [],
        "leaf": None,
        "result": None
    }

    table_pa = root_pa
    for lvl in reversed(range(levels)):  # levels-1 .. 0
        idx = (va >> (12 + 9 * lvl)) & 0x1FF
        pte_pa = table_pa + idx * 8
        pte_va = (pte_pa + hhdm_off) if hhdm_off is not None else None

        if pte_va is None:
            return {"ok": False, "reason": "HHDM offset unknown; set it with mmu-set-hhdm or expose kernel symbol hhdm_offset."}

        try:
            pte_val = _read_u64(process, pte_va)
        except Exception as e:
            return {"ok": False, "reason": f"Failed reading PTE at { _format_addr(pte_va) }: {e}"}

        step = {
            "level": lvl,
            "index": idx,
            "pte_pa": pte_pa,
            "pte_va": pte_va,
            "pte": pte_val,
            "flags": _pte_flags_str(pte_val),
            "valid": _pte_is_valid(pte_val),
            "leaf": _pte_is_leaf(pte_val),
        }
        path["steps"].append(step)

        if not step["valid"]:
            return {"ok": False, "reason": f"Invalid PTE at level {lvl}, index {idx} (PTE={_format_addr(pte_val)})", "path": path}

        if step["leaf"]:
            page_size = 1 << (12 + 9 * lvl)
            pte_ppn = _pte_ppn(pte_val)
            pa = (pte_ppn << 12) | (va & (page_size - 1))
            path["leaf"] = {
                "level": lvl,
                "page_size": page_size,
                "pte_ppn": pte_ppn
            }
            path["result"] = {
                "pa": pa,
                "va_for_pte": pte_va,
                "va": va
            }
            return {"ok": True, "path": path}

        next_table_pa = _pte_ppn(pte_val) << 12
        table_pa = next_table_pa

    return {"ok": False, "reason": "Walk terminated without finding a leaf PTE", "path": path}

def mmu_walk(debugger, command, exe_ctx, result, internal_dict): # noqa: C901
    """
    mmu-walk <va> [--hhdm <offset>] [--satp <val>] [--sv {39,48,57}] [--verbose]
    Walk the current RISC-V page tables and resolve VA -> PA.

    Examples:
      (lldb) mmu-walk 0xffffffff800112ba
      (lldb) mmu-walk 0x1000 --verbose
      (lldb) mmu-walk 0x1234 --hhdm 0xffffffc000000000
    """
    try:
        args = shlex.split(command)
        if not args:
            _err(result, "usage: mmu-walk <va> [--hhdm <offset>] [--satp <val>] [--sv {39,48,57}] [--verbose]")
            return

        va = None
        hhdm = None
        satp_val = None
        override_sv = None
        verbose = False

        i = 0
        while i < len(args):
            a = args[i]
            if va is None:
                try:
                    va = int(a, 0)
                    i += 1
                    continue
                except ValueError:
                    pass
            if a == "--hhdm" and i + 1 < len(args):
                hhdm = int(args[i + 1], 0)
                i += 2
            elif a == "--satp" and i + 1 < len(args):
                satp_val = int(args[i + 1], 0)
                i += 2
            elif a == "--sv" and i + 1 < len(args):
                override_sv = int(args[i + 1], 0)
                i += 2
            elif a == "--verbose":
                verbose = True
                i += 1
            else:
                _err(result, f"unknown argument: {a}")
                return

        if va is None:
            _err(result, "missing <va>")
            return

        target = debugger.GetSelectedTarget()
        process = exe_ctx.process or target.process

        if satp_val is None:
            satp_val = _read_reg(exe_ctx, "satp")
            if satp_val is None:
                _err(result, "failed to read satp; provide with --satp")
                return

        if hhdm is None:
            off, ok = _get_hhdm_offset(target, process)
            if ok:
                hhdm = off

        mode_name, levels, asid, root_ppn, mode_num = _satp_decode(satp_val, override_sv)
        header = f"SATP={_format_addr(satp_val)} mode={mode_name} levels={levels} asid=0x{asid:x} root_ppn=0x{root_ppn:x}"
        if hhdm is not None:
            header += f" hhdm={_format_addr(hhdm)}"
        _append(result, header)

        walk = _walk_impl(target, process, satp_val, va, hhdm, override_sv, verbose)
        if not walk.get("ok"):
            reason = walk.get("reason", "unknown error")
            _err(result, reason)
            # Still print steps if available for diagnostics.
            path = walk.get("path")
            if path:
                _print_steps(result, path)
            return

        path = walk["path"]
        _print_steps(result, path)

        leaf = path["leaf"]
        res = path["result"]
        pa = res["pa"]
        page_sz = leaf["page_size"]
        _append(result, f"RESOLVED: VA={_format_addr(va)} -> PA={_format_addr(pa)} page_size={page_sz//1024} KiB (level {leaf['level']})")
    except Exception as e:
        _err(result, f"mmu-walk failed: {e}")

def _print_steps(result, path):
    root_pa = path["root_pa"]
    root_va = path["root_va"]
    _append(result, f"root table: PA={_format_addr(root_pa)} VA={( _format_addr(root_va) if root_va is not None else 'UNKNOWN')}")
    for s in path["steps"]:
        line = f"L{s['level']} idx={s['index']:3d} PTE@PA={_format_addr(s['pte_pa'])} VA={( _format_addr(s['pte_va']) )} PTE={_format_addr(s['pte'])} [{s['flags']}]"
        if s["leaf"]:
            line += " LEAF"
        _append(result, line)

def mmu_info(debugger, command, exe_ctx, result, internal_dict):
    """
    mmu-info [--sv {39,48,57}] [--satp <val>]
    Print basic paging info from satp and root page table locations.
    """
    try:
        args = shlex.split(command)
        override_sv = None
        satp_val = None

        i = 0
        while i < len(args):
            a = args[i]
            if a == "--sv" and i + 1 < len(args):
                override_sv = int(args[i + 1], 0)
                i += 2
            elif a == "--satp" and i + 1 < len(args):
                satp_val = int(args[i + 1], 0)
                i += 2
            else:
                _err(result, f"unknown argument: {a}")
                return

        target = debugger.GetSelectedTarget()
        process = exe_ctx.process or target.process

        if satp_val is None:
            satp_val = _read_reg(exe_ctx, "satp")
            if satp_val is None:
                _err(result, "failed to read satp; provide with --satp")
                return

        hhdm, hhdm_ok = _get_hhdm_offset(target, process)
        mode_name, levels, asid, root_ppn, mode_num = _satp_decode(satp_val, override_sv)
        root_pa = root_ppn << 12
        root_va = (root_pa + hhdm) if hhdm_ok else None

        _append(result, f"SATP={_format_addr(satp_val)}")
        _append(result, f"  mode={mode_name} levels={levels} asid=0x{asid:x} root_ppn=0x{root_ppn:x}")
        _append(result, f"  root_pa={_format_addr(root_pa)}")
        _append(result, f"  root_va={( _format_addr(root_va) if root_va is not None else 'UNKNOWN' )}")
        _append(result, f"  hhdm={( _format_addr(hhdm) if hhdm_ok else 'UNKNOWN' )}")
    except Exception as e:
        _err(result, f"mmu-info failed: {e}")

def mmu_set_hhdm(debugger, command, exe_ctx, result, internal_dict):
    """
    mmu-set-hhdm <offset>
    Set or override the HHDM offset used to read page tables.
    """
    try:
        args = shlex.split(command)
        if len(args) != 1:
            _err(result, "usage: mmu-set-hhdm <offset>")
            return
        off = int(args[0], 0)
        _set_hhdm_offset(off)
        _append(result, f"hhdm set to {_format_addr(off)}")
    except Exception as e:
        _err(result, f"mmu-set-hhdm failed: {e}")

def mmu_dump(debugger, command, exe_ctx, result, internal_dict): # noqa: C901
    """
    mmu-dump [--path <i0,i1,...>] [--all] [--satp <val>] [--sv {39,48,57}] [--hhdm <offset>]
    Dump PTEs of a page-table at root or at the table reached by the index path.
    By default, only valid entries are printed unless --all is given.

    Examples:
      (lldb) mmu-dump
      (lldb) mmu-dump --path 255
      (lldb) mmu-dump --path 255,12 --all
    """
    try:
        args = shlex.split(command)
        path = []
        show_all = False
        satp_val = None
        override_sv = None
        hhdm = None

        i = 0
        while i < len(args):
            a = args[i]
            if a == "--path" and i + 1 < len(args):
                raw = args[i + 1]
                path = [int(x, 0) for x in raw.split(",") if x]
                i += 2
            elif a == "--all":
                show_all = True
                i += 1
            elif a == "--satp" and i + 1 < len(args):
                satp_val = int(args[i + 1], 0)
                i += 2
            elif a == "--sv" and i + 1 < len(args):
                override_sv = int(args[i + 1], 0)
                i += 2
            elif a == "--hhdm" and i + 1 < len(args):
                hhdm = int(args[i + 1], 0)
                i += 2
            else:
                _err(result, f"unknown argument: {a}")
                return

        target = debugger.GetSelectedTarget()
        process = exe_ctx.process or target.process

        if satp_val is None:
            satp_val = _read_reg(exe_ctx, "satp")
            if satp_val is None:
                _err(result, "failed to read satp; provide with --satp")
                return

        if hhdm is None:
            off, ok = _get_hhdm_offset(target, process)
            if ok:
                hhdm = off
            else:
                _err(result, "HHDM offset unknown; set it with mmu-set-hhdm or expose kernel symbol hhdm_offset.")
                return

        mode_name, levels, asid, root_ppn, mode_num = _satp_decode(satp_val, override_sv)
        if levels == 0:
            _err(result, f"Paging mode is {mode_name}; cannot dump.")
            return

        table_pa = root_ppn << 12
        lvl_from_top = levels - 1

        for depth, idx in enumerate(path):
            if idx < 0 or idx >= 512:
                _err(result, f"invalid index in path at depth {depth}: {idx}")
                return
            pte_pa = table_pa + idx * 8
            pte_va = pte_pa + hhdm
            pte_val = _read_u64(process, pte_va)
            if not _pte_is_valid(pte_val):
                _append(result, f"path step depth={depth} L{lvl_from_top} idx={idx}: invalid PTE { _format_addr(pte_val) }")
                return
            if _pte_is_leaf(pte_val):
                _append(result, f"path step depth={depth} L{lvl_from_top} idx={idx}: reached LEAF PTE { _format_addr(pte_val) } [{ _pte_flags_str(pte_val) }], cannot descend further")
                return
            table_pa = _pte_ppn(pte_val) << 12
            lvl_from_top -= 1

        table_va = table_pa + hhdm
        _append(result, f"dumping table at PA={_format_addr(table_pa)} VA={_format_addr(table_va)} (level L{lvl_from_top}) mode={mode_name}")

        count = 0
        for iidx in range(512):
            pte_pa = table_pa + iidx * 8
            pte_va = table_va + iidx * 8
            pte_val = _read_u64(process, pte_va)
            valid = _pte_is_valid(pte_val)
            leaf = _pte_is_leaf(pte_val)
            if not show_all and not valid:
                continue
            flags = _pte_flags_str(pte_val)
            target_pa = (_pte_ppn(pte_val) << 12) if valid else 0
            line = f"idx={iidx:3d} PTE@PA={_format_addr(pte_pa)} VA={_format_addr(pte_va)} PTE={_format_addr(pte_val)} [{flags}]"
            if valid:
                line += f" -> next_pa={_format_addr(target_pa)}"
                if leaf:
                    line += " LEAF"
            _append(result, line)
            count += 1

        if not count:
            _append(result, "(no entries to show)")
    except Exception as e:
        _err(result, f"mmu-dump failed: {e}")

def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('command script add -f mmu.mmu_walk mmu-walk')
    debugger.HandleCommand('command script add -f mmu.mmu_info mmu-info')
    debugger.HandleCommand('command script add -f mmu.mmu_set_hhdm mmu-set-hhdm')
    debugger.HandleCommand('command script add -f mmu.mmu_dump mmu-dump')
    print("mmu commands installed: mmu-info, mmu-walk, mmu-dump, mmu-set-hhdm")

# trap_origin.py  –  “trap-origin” LLDB command for RISC-V gizmOS
# Usage inside LLDB:
#   (lldb) command script import trap_origin.py
#   (lldb) trap-origin
#
# Finds the frame for trap_vector, reads the first 8 bytes of the trap stack
# (saved ra), resolves that address to a symbol / source line and prints it.

import lldb


def trap_origin(debugger, command, exe_ctx, result, internal_dict):
    target  = debugger.GetSelectedTarget()
    process = exe_ctx.process or target.process
    thread  = exe_ctx.thread or process.GetSelectedThread()

    if not thread or not thread.IsValid():
        result.SetError("no running thread")
        return

    # 1. locate the trap_vector frame ----------------------------------------
    trap_f = None
    for f in thread:
        name = f.GetFunctionName() or f.GetSymbol().GetName()
        if name == "trap_vector":
            trap_f = f
            break

    if trap_f is None:
        result.SetError("trap_vector frame not found in this thread")
        return

    # 2. read sp and fetch the saved ra at 0(sp) ------------------------------
    sp_reg = trap_f.FindRegister("sp")
    if not sp_reg or not sp_reg.IsValid():
        result.SetError("could not read sp in trap_vector frame")
        return

    sp_val = sp_reg.GetValueAsUnsigned()
    err    = lldb.SBError()
    saved_ra = process.ReadUnsignedFromMemory(sp_val, 8, err)

    if err.Fail():
        result.SetError(f"memory read failed: {err.GetCString()}")
        return

    # 3. resolve the address to a symbol / line -------------------------------
    addr      = target.ResolveLoadAddress(saved_ra)
    sym_ctx   = addr.GetSymbolContext(lldb.eSymbolContextEverything)

    # build nice output
    out  = f"saved ra  : 0x{saved_ra:016x}\n"
    if sym_ctx.GetFunction():
        func = sym_ctx.GetFunction()
        offset = saved_ra - func.GetStartAddress().GetLoadAddress(target)
        out += f"function  : {func.GetDisplayName()} + 0x{offset:x}\n"
    if sym_ctx.GetLineEntry().IsValid():
        le = sym_ctx.GetLineEntry()
        out += f"location  : {le.GetFileSpec().GetFilename()}:{le.GetLine()}\n"
    result.AppendMessage(out)


def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand(
        'command script add -f trap_origin.trap_origin trap-origin')
    print("trap-origin command installed – run “trap-origin” to locate the \
origin of the current trap.")

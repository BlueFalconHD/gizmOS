import os
import lldb

def _run_cmd(debugger: lldb.SBDebugger, command: str, result):
    ci = debugger.GetCommandInterpreter()
    ro = lldb.SBCommandReturnObject()
    ci.HandleCommand(command, ro)
    if not ro.Succeeded():
        err = ro.GetError()
        if result is not None and err:
            result.SetError(err)
        raise RuntimeError(err or f"Command failed: {command}")
    return ro.GetOutput()

def reattach(debugger, command, exe_ctx, result, internal_dict):
    """
    reattach
    Attempt to reconnect to the debug server (e.g., QEMU remote debug stub).
    Usage: reattach
    """
    try:
        target = debugger.GetSelectedTarget()
        process = target.GetProcess() if target and target.IsValid() else None

        # Step 1: Detach cleanly if we are attached to a running/stopped process.
        if process and process.IsValid():
            state = process.GetState()
            if state not in (lldb.eStateExited, lldb.eStateDetached, lldb.eStateInvalid):
                result.AppendMessage("Detaching from existing debug session...")
                _run_cmd(debugger, 'process detach', result)

        # Step 2: Recreate a fresh target and connect to the remote stub.
        kernel_path = '/Users/hayes/Stores/repos/bluefalconhd/gizmOS/kernel/bin-riscv64/kernel'
        if target and target.IsValid():
            # Ensure we don't carry over a dead process
            _run_cmd(debugger, 'target delete', result)

        _run_cmd(debugger, f'file {kernel_path}', result)

        # Prefer explicit process connect to the gdb-remote plugin
        result.AppendMessage("Reconnecting to gdb-remote at localhost:1234...")
        try:
            _run_cmd(debugger, 'process connect -p gdb-remote connect://localhost:1234', result)
        except Exception:
            # Fallback to the older alias if needed
            _run_cmd(debugger, 'gdb-remote localhost:1234', result)

        # Confirm we actually have a live process
        new_target = debugger.GetSelectedTarget()
        new_process = new_target.GetProcess() if new_target and new_target.IsValid() else None
        if not (new_process and new_process.IsValid() and new_process.GetState() != lldb.eStateInvalid):
            raise RuntimeError('Failed to connect to remote stub: no current process')

        # Optional: add extra symbols if present (guard to avoid noisy errors)
        dSYM_path = 'limine/limine.dSYM/Contents/Resources/DWARF/limine'
        if os.path.exists(dSYM_path):
            try:
                _run_cmd(debugger, f'target symbols add {dSYM_path}', None)
            except Exception:
                # Best-effort; ignore if module doesn't match
                pass

        # Optionally, (re)set breakpoints of interest
        _run_cmd(debugger, 'breakpoint set --name exception_handler', None)
        _run_cmd(debugger, 'breakpoint set --name dbg_internal', None)

        result.AppendMessage("Reattachment complete.")
    except Exception as e:
        result.SetError(f"reattach failed: {e}")

def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('command script add -f reattach.reattach reattach')
    print("reattach command installed.")

# rs.py  –  step once command
# Usage inside LLDB:
#   (lldb) command script import rs.py
#   (lldb) rs
#
# Steps one instruction, without letting interrupts cause the program to continue.

import lldb

def rs(debugger, command, exe_ctx, result, internal_dict):
    # just run thread step-inst --count 1 --step-over 1
    target  = debugger.GetSelectedTarget()
    process = exe_ctx.process or target.process
    thread  = exe_ctx.thread or process.GetSelectedThread()
    if not thread or not thread.IsValid():
        result.SetError("no running thread")
        return

    # Execute the exact LLDB command that performs a single instruction step
    # while stepping over calls (equivalent to “thread step-inst --count 1 --step-over 1”).
    # Using the command interpreter lets us surface any error or textual output
    # through the provided `result` object.
    ci  = debugger.GetCommandInterpreter()
    cro = lldb.SBCommandReturnObject()
    ci.HandleCommand("thread step-inst --count 1 --step-over 1", cro)

    if not cro.Succeeded():
        result.SetError(cro.GetError())
    else:
        output = cro.GetOutput()
        if output:
            result.AppendMessage(output)


def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand(
        'command script add -f rs.rs rs')
    print("rs command installed – run “rs” to step once without letting interrupts \
    cause the program to continue.")

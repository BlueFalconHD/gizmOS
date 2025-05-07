import lldb

def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('file /Users/hayes/Stores/repos/bluefalconhd/gizmOS/kernel/bin-riscv64/kernel')
    debugger.HandleCommand('command script import trap_origin.py')
    debugger.HandleCommand('gdb-remote localhost:1234')
    debugger.HandleCommand('breakpoint set --name exception_handler')

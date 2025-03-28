import lldb

def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand('file kernel/bin-riscv64/kernel')
    debugger.HandleCommand('gdb-remote localhost:1234')
    debugger.HandleCommand('breakpoint set --name trap_vector')

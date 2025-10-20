import lldb


def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand(
        "file /Users/hayes/Stores/repos/bluefalconhd/gizmOS/kernel/bin-riscv64/kernel"
    )
    debugger.HandleCommand("command script import lldb/trap_origin.py")
    debugger.HandleCommand("command script import lldb/rs.py")
    debugger.HandleCommand("command script import lldb/disassembly_mode.py")
    debugger.HandleCommand("command script import lldb/mmu.py")
    debugger.HandleCommand("command script import lldb/reattach.py")
    debugger.HandleCommand("gdb-remote localhost:1234")
    debugger.HandleCommand("breakpoint set --name exception_handler")
    debugger.HandleCommand("breakpoint set --name dbg_internal")
    debugger.HandleCommand(
        "target symbols add limine/limine.dSYM/Contents/Resources/DWARF/limine"
    )

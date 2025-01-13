#!/usr/bin/env fish

# run LLDB with command `gdb-remote localhost:1234` and then file `kernel/bin-aarch64/kernel`
lldb "kernel/bin-aarch64/kernel" -o "gdb-remote localhost:1234"

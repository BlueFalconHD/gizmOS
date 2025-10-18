#pragma once

// Syscall numbers used in usertrap dispatch (a7 register)
// Keep small and reserved range for notifications.
#define SYSCALL_NOTIF_REGISTER   0x90
#define SYSCALL_NOTIF_UNREGISTER 0x91
#define SYSCALL_NOTIF_DONE       0x100

// Simple printing syscall: prints signed int in a0 to UART and terminal
#define SYSCALL_PRINT_INT        0x10


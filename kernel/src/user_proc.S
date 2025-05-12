.section .text
.align 4

.global proc_ecall7_start
proc_ecall7_start:
    addi a7, x0, 7  // Load immediate 7 into a7 (syscall number)
    ecall           // Trigger system call
    jal  x0, .-4    // Jump back 4 bytes (to the ecall instruction), creating a loop
.global proc_ecall7_end
proc_ecall7_end:

.global proc_ecall8_start
proc_ecall8_start:
    addi a7, x0, 8  // Load immediate 8 into a7 (syscall number)
    ecall           // Trigger system call
    jal  x0, .-4    // Jump back 4 bytes (to the ecall instruction), creating a loop
.global proc_ecall8_end
proc_ecall8_end:

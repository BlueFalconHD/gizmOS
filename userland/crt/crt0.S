.section .text
.globl _start
_start:
  # assume sp is set by kernel to top of user stack
  call main
  # on return, a0 has return code
  mv a0, a0
  li a7, 0x02  # SYSCALL_EXIT
  ecall
1: j 1b


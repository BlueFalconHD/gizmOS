.section .text
.align 4

.globl user_keynotify_start
user_keynotify_start:
  # Register notification handler for keypress (type = 1)
  li a0, 1              # NOTIF_TYPE_KEYPRESS
  la a1, key_handler    # handler VA
  li a2, 0              # arg
  li a3, 0              # flags
  li a7, 0x90           # SYSCALL_NOTIF_REGISTER
  ecall

1:                      # idle loop; timer ticks will drive notifications
  j 1b

# Notification handler. Kernel sets:
#   a0 = type, a1 = payload uva, a2 = len, a3 = user arg
# Returns by performing ecall with a7 = SYSCALL_NOTIF_DONE (0x100)
key_handler:
  # if payload length >= 1, print first byte (keycode)
  beqz a2, .Ldone

  # if type != NOTIF_TYPE_KEYPRESS (1), just return
  li t0, 1
  bne a0, t0, .Ldone

  lbu t0, 0(a1)         # t0 = keycode
  mv a0, t0             # a0 = int to print
  li a7, 0x10           # SYSCALL_PRINT_INT
  ecall
.Ldone:
  li a7, 0x100          # SYSCALL_NOTIF_DONE
  ecall

.globl user_keynotify_end
user_keynotify_end:

.global goldfish_get_time
.global goldfish_get_alarm
.global goldfish_set_alarm
.global goldfish_get_clear_interrupt
.global goldfish_set_clear_interrupt

.equ GOLDFISH_RTC_BASE, 0x101000
.equ TIME_LOW_REGISTER, 0x0
.equ TIME_HIGH_REGISTER, 0x4
.equ ALARM_LOW_REGISTER, 0x8
.equ ALARM_HIGH_REGISTER, 0xC
.equ CLEAR_INTERRUPT_REGISTER, 0x10

goldfish_get_time:
.cfi_startproc
    li  t0, GOLDFISH_RTC_BASE

    lw  a0, TIME_LOW_REGISTER(t0)
    lw  a1, TIME_HIGH_REGISTER(t0)

    // Merge into one 64-bit value
    slli a1, a1, 32
    or   a0, a0, a1

    ret

.cfi_endproc

goldfish_get_alarm:
.cfi_startproc
    li  t0, GOLDFISH_RTC_BASE

    lw  a0, ALARM_LOW_REGISTER(t0)
    lw  a1, ALARM_HIGH_REGISTER(t0)

    // Merge into one 64-bit value
    slli a1, a1, 32
    or   a0, a0, a1

    ret

.cfi_endproc
goldfish_set_alarm:
.cfi_startproc
    li  t0, GOLDFISH_RTC_BASE

    // Split into high and low parts
    srli a1, a0, 32
    lw   a2, ALARM_LOW_REGISTER(t0)
    sw   a2, ALARM_LOW_REGISTER(t0)
    sw   a1, ALARM_HIGH_REGISTER(t0)

    ret

.cfi_endproc

goldfish_get_clear_interrupt:
.cfi_startproc
    li  t0, GOLDFISH_RTC_BASE

    lw  a0, CLEAR_INTERRUPT_REGISTER(t0)

    // Merge into one 64-bit value
    slli a1, a1, 32
    or   a0, a0, a1

    ret

.cfi_endproc

goldfish_set_clear_interrupt:
.cfi_startproc
    li  t0, GOLDFISH_RTC_BASE

    // Split into high and low parts
    srli a1, a0, 32
    lw   a2, CLEAR_INTERRUPT_REGISTER(t0)
    sw   a2, CLEAR_INTERRUPT_REGISTER(t0)
    sw   a1, CLEAR_INTERRUPT_REGISTER(t0)

    ret
.cfi_endproc

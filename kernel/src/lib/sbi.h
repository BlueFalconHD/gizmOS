#pragma once
#include <lib/macros.h>
#include <stdint.h>

#include <stddef.h>
#include <stdint.h>

struct sbiret {
    long error;
    long value;
};

#define SBI_SUCCESS                 ((long)0)
#define SBI_ERR_FAILED              ((long)-1)
#define SBI_ERR_NOT_SUPPORTED       ((long)-2)
#define SBI_ERR_INVALID_PARAM       ((long)-3)
#define SBI_ERR_DENIED              ((long)-4)
#define SBI_ERR_INVALID_ADDRESS     ((long)-5)
#define SBI_ERR_ALREADY_AVAILABLE   ((long)-6)
#define SBI_ERR_ALREADY_STARTED     ((long)-7)
#define SBI_ERR_ALREADY_STOPPED     ((long)-8)

extern struct sbiret sbicall(int eid, int fid, ...);

// base
#define SBI_EID_BASE        0x10
#define SBI_EID_BASE_FID_SPEC_VERSION 0x0
#define SBI_EID_BASE_FID_IMPL_ID 0x1
#define SBI_EID_BASE_FID_IMPL_VERSION 0x2
#define SBI_EID_BASE_FID_PROBE_EXT 0x3
#define SBI_EID_BASE_FID_GET_MVENDORID 0x4
#define SBI_EID_BASE_FID_GET_MARCHID 0x5
#define SBI_EID_BASE_FID_GET_MIMPID 0x6

G_INLINE struct sbiret sbi_get_spec_version(void) {
  return sbicall(SBI_EID_BASE, SBI_EID_BASE_FID_SPEC_VERSION);
}

G_INLINE struct sbiret sbi_get_impl_id(void) {
  return sbicall(SBI_EID_BASE, SBI_EID_BASE_FID_IMPL_ID);
}

G_INLINE struct sbiret sbi_get_impl_version(void) {
  return sbicall(SBI_EID_BASE, SBI_EID_BASE_FID_IMPL_VERSION);
}

G_INLINE struct sbiret sbi_probe_extension(long extension_id) {
  return sbicall(SBI_EID_BASE, SBI_EID_BASE_FID_PROBE_EXT, extension_id);
}

G_INLINE struct sbiret sbi_get_mvendorid(void) {
  return sbicall(SBI_EID_BASE, SBI_EID_BASE_FID_GET_MVENDORID);
}

G_INLINE struct sbiret sbi_get_marchid(void) {
  return sbicall(SBI_EID_BASE, SBI_EID_BASE_FID_GET_MARCHID);
}

G_INLINE struct sbiret sbi_get_mimpid(void) {
  return sbicall(SBI_EID_BASE, SBI_EID_BASE_FID_GET_MIMPID);
}

#define SBI_IMP_ID_BBL 0x0
#define SBI_IMP_ID_OPENSBI 0x1
#define SBI_IMP_ID_XVISOR 0x2
#define SBI_IMP_ID_KVM 0x3
#define SBI_IMP_ID_RUSTSBI 0x4
#define SBI_IMP_ID_DIOSIX 0x5
#define SBI_IMP_ID_COFFER 0x6

// hart state management
#define SBI_EID_HSM         0x48534d
#define SBI_EID_HSM_FID_START 0x0
#define SBI_EID_HSM_FID_STOP 0x1
#define SBI_EID_HSM_FID_STATUS 0x2
#define SBI_EID_HSM_FID_SUSPEND 0x3

G_INLINE struct sbiret sbi_hart_start(unsigned long hartid, unsigned long start_addr,
                             unsigned long opaque) {
  return sbicall(SBI_EID_HSM, SBI_EID_HSM_FID_START, hartid, start_addr,
                 opaque);
}

G_INLINE struct sbiret sbi_hart_stop(void) {
  return sbicall(SBI_EID_HSM, SBI_EID_HSM_FID_STOP);
}

G_INLINE struct sbiret sbi_hart_get_status(unsigned long hartid) {
  return sbicall(SBI_EID_HSM, SBI_EID_HSM_FID_STATUS, hartid);
}

G_INLINE struct sbiret sbi_hart_suspend(uint32_t suspend_type, unsigned long resume_addr,
                               unsigned long opaque) {
  return sbicall(SBI_EID_HSM, SBI_EID_HSM_FID_SUSPEND, suspend_type,
                 resume_addr, opaque);
}

// system reset
#define SBI_EID_SRST        0x53525354
#define SBI_EID_SRST_FID_SYSTEM_RESET 0x0

G_INLINE struct sbiret sbi_system_reset(uint32_t reset_type, uint32_t reset_reason) {
  return sbicall(SBI_EID_SRST, SBI_EID_SRST_FID_SYSTEM_RESET, reset_type,
                 reset_reason);
}

#define SBI_SRST_TYPE_SHUTDOWN 0x0
#define SBI_SRST_TYPE_COLD_REBOOT 0x1
#define SBI_SRST_TYPE_WARM_REBOOT 0x2

#define SBI_SRST_REASON_NONE 0x0
#define SBI_SRST_REASON_FAILURE 0x1

// timer
#define SBI_EID_TIME        0x54494D45
#define SBI_EID_TIME_FID_SET_TIMER 0x0

G_INLINE struct sbiret sbi_set_timer(uint64_t stime_value) {
  return sbicall(SBI_EID_TIME, SBI_EID_TIME_FID_SET_TIMER, stime_value);
}

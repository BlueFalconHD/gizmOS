#include "rtc.h"
#include <lib/result.h>
#include <physical_alloc.h>

#define TIME_LOW_REGISTER 0x0
#define TIME_HIGH_REGISTER 0x4
#define ALARM_LOW_REGISTER 0x8
#define ALARM_HIGH_REGISTER 0xC
#define CLEAR_INTERRUPT_REGISTER 0x10

RESULT_TYPE(rtc_t *) make_rtc(uint64_t base) {
  rtc_t *rtc = (rtc_t *)alloc_page();
  if (!rtc) {
    return RESULT_FAILURE(RESULT_NOMEM);
  }
  rtc->base = base;
  rtc->is_initialized = false;

  return RESULT_SUCCESS(rtc);
}

g_bool rtc_init(rtc_t *rtc) {
  if (!rtc) {
    return false;
  }

  rtc->is_initialized = true;
  return true;
}

uint64_t rtc_get_time(rtc_t *rtc) {
  if (!rtc || !rtc->is_initialized) {
    return 0;
  }

  volatile uint32_t *r = (volatile uint32_t *)rtc->base;
  uint32_t low = r[TIME_LOW_REGISTER / 4];
  uint32_t high = r[TIME_HIGH_REGISTER / 4];

  // Merge into one 64-bit value
  uint64_t time = ((uint64_t)high << 32) | low;
  return time;
}

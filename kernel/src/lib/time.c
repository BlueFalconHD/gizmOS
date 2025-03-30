#include "device/shared.h"
#include <device/rtc.h>
#include <lib/fmt.h>
#include <lib/print.h>
#include <lib/str.h>
#include <lib/time.h>
#include <physical_alloc.h>
#include <stdint.h>

// Global variable that represents the time that sleep should end
static uint64_t sleep_end = 0;

// Array of days in each month (non-leap year)
static const uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30,
                                        31, 31, 30, 31, 30, 31};

// Helper function to get days in a specific month considering leap years
static uint8_t get_days_in_month(month_t month, year_t year) {
  if (month == FEBRUARY && IS_LEAP_YEAR(year)) { // February in leap year
    return 29;
  }
  return days_in_month[month];
}

void unix_time_ns_to_time(uint64_t unix_time, time_t *tm) {
  // Convert nanoseconds to seconds
  uint32_t seconds = unix_time / 1000000000ULL;

  // Convert remaining nanoseconds to milliseconds
  uint32_t milliseconds = (unix_time % 1000000000ULL) / 1000000ULL;

  // Convert seconds to time structure
  unix_time_to_time(seconds, tm);
}

void unix_time_to_time(uint32_t unix_time, time_t *tm) {
  uint32_t days, seconds_remaining;
  year_t year;
  month_t month;

  // Calculate days since epoch
  days = unix_time / SECONDS_IN_DAY;
  seconds_remaining = unix_time % SECONDS_IN_DAY;

  // Calculate time
  tm->hours = seconds_remaining / SECONDS_IN_HOUR;
  seconds_remaining = seconds_remaining % SECONDS_IN_HOUR;
  tm->minutes = seconds_remaining / SECONDS_IN_MINUTE;
  tm->seconds = seconds_remaining % SECONDS_IN_MINUTE;

  // Calculate year
  year = EPOCH_YEAR;

  // Adjust for years
  while (days >= (IS_LEAP_YEAR(year) ? DAYS_IN_LEAP_YEAR : DAYS_IN_YEAR)) {
    days -= (IS_LEAP_YEAR(year) ? DAYS_IN_LEAP_YEAR : DAYS_IN_YEAR);
    year++;
  }

  tm->year = year;

  // Calculate month and day
  for (month = JANUARY; month <= DECEMBER; month++) {
    uint8_t dim = get_days_in_month(month, year);
    if (days < dim)
      break;
    days -= dim;
  }

  tm->month = month;
  tm->day = days + 1;

  // Calculate day of week (1/1/1970 was a Thursday)
  tm->weekday =
      (unix_time / SECONDS_IN_DAY + 4) % 7; // Thursday + 4 = 4 (Thursday)
}

uint32_t time_to_unix_time(const time_t *tm) {
  uint32_t unix_time = 0;
  year_t year = tm->year;

  // Calculate seconds from date
  // First calculate days from years since epoch
  for (year_t y = EPOCH_YEAR; y < year; y++) {
    unix_time += IS_LEAP_YEAR(y) ? DAYS_IN_LEAP_YEAR : DAYS_IN_YEAR;
  }

  // Add days from months
  for (month_t m = JANUARY; m < tm->month; m++) {
    unix_time += get_days_in_month(m, year);
  }

  // Add days in current month
  unix_time += tm->day - 1;

  // Convert days to seconds and add time components
  unix_time = unix_time * SECONDS_IN_DAY + tm->hours * SECONDS_IN_HOUR +
              tm->minutes * SECONDS_IN_MINUTE + tm->seconds;

  return unix_time;
}

static const char *month_names[] = {
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December"};

static const char *weekday_names[] = {"Sunday",    "Monday",   "Tuesday",
                                      "Wednesday", "Thursday", "Friday",
                                      "Saturday"};

void time_to_string(const time_t *tm, char *buf) {
  const char *month = month_names[tm->month];
  const char *weekday = weekday_names[tm->weekday];

  char *f = format(
      "%{type: str} %{type:str} %{type: uint}, %{type: uint} %{type: "
      "uint}:%{type: uint}:%{type: uint}",
      weekday, month, tm->day, tm->year, tm->hours, tm->minutes, tm->seconds);

  strncopy(buf, f, strlen(f) + 1);
  free_page(f);
}

void sleep_s(uint32_t seconds) {
  // Convert time to ns
  uint64_t ns = (uint64_t)seconds * 1000000000ULL;
  uint64_t current_time = shared_rtc_get_time();

  // Set the end time
  sleep_end = current_time + ns;

  // Wait until the end time
  while (shared_rtc_get_time() < sleep_end) {
    asm volatile("wfi");
  }
}

void sleep_ms(uint32_t milliseconds) {
  // Convert time to ns
  uint64_t ns = (uint64_t)milliseconds * 1000000ULL;
  uint64_t current_time = shared_rtc_get_time();

  // Set the end time
  sleep_end = current_time + ns;

  // Wait until the end time
  while (shared_rtc_get_time() < sleep_end) {
    asm volatile("wfi");
  }
}

void sleep_us(uint32_t microseconds) {
  // Convert time to ns
  uint64_t ns = (uint64_t)microseconds * 1000ULL;
  uint64_t current_time = shared_rtc_get_time();

  // Set the end time
  sleep_end = current_time + ns;

  // Wait until the end time
  while (shared_rtc_get_time() < sleep_end) {
    asm volatile("wfi");
  }
}

void sleep_ns(uint32_t nanoseconds) {
  uint64_t current_time = shared_rtc_get_time();

  // Set the end time
  sleep_end = current_time + nanoseconds;

  // Wait until the end time
  while (shared_rtc_get_time() < sleep_end) {
    asm volatile("wfi");
  }
}

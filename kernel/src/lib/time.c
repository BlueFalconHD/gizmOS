// #include <stdint.h>
// #include "time.h"
// #include <device/rtc.h>
// #include <lib/str.h>
// #include <lib/fmt.h>
// #include "memory.h"

// // global variable that represents the time that sleep should end
// static uint32_t sleep_end = 0;

// // Array of days in each month (non-leap year)
// static const uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// #define EPOCH_YEAR 1970

// // Helper function to get days in a specific month considering leap years
// static uint8_t get_days_in_month(uint8_t month, uint16_t year) {
//     if (month == 1 && IS_LEAP_YEAR(year)) { // February in leap year
//         return 29;
//     }
//     return days_in_month[month];
// }

// void unix_time_to_time(uint32_t unix_time, struct tm *time) {
//     uint32_t days, seconds_remaining, years;
//     uint16_t year;
//     uint8_t month;

//     // Calculate days since epoch
//     days = unix_time / SECONDS_IN_DAY;
//     seconds_remaining = unix_time % SECONDS_IN_DAY;

//     // Calculate time
//     time->tm_hour = seconds_remaining / SECONDS_IN_HOUR;
//     seconds_remaining = seconds_remaining % SECONDS_IN_HOUR;
//     time->tm_min = seconds_remaining / SECONDS_IN_MINUTE;
//     time->tm_sec = seconds_remaining % SECONDS_IN_MINUTE;

//     // Calculate year
//     year = EPOCH_YEAR;
//     years = days / 365;
//     days -= years * 365;

//     // Adjust for leap years
//     for (uint16_t y = EPOCH_YEAR; y < EPOCH_YEAR + years; y++) {
//         if (IS_LEAP_YEAR(y)) {
//             days--;
//         }
//     }

//     // Adjust year if we've gone too far
//     while (days >= (IS_LEAP_YEAR(year) ? DAYS_IN_LEAP_YEAR : DAYS_IN_YEAR)) {
//         days -= (IS_LEAP_YEAR(year) ? DAYS_IN_LEAP_YEAR : DAYS_IN_YEAR);
//         year++;
//     }

//     time->tm_year = year - 1900;
//     time->tm_yday = days;

//     // Calculate month and day
//     for (month = 0; month < 12; month++) {
//         uint8_t dim = get_days_in_month(month, year);
//         if (days < dim)
//             break;
//         days -= dim;
//     }

//     time->tm_mon = month;
//     time->tm_mday = days + 1;

//     // Calculate day of week (1/1/1970 was a Thursday)
//     time->tm_wday = (unix_time / SECONDS_IN_DAY + 4) % 7;

//     // Set DST to 0 (not handled)
//     time->tm_isdst = 0;
// }

// uint32_t time_to_unix_time(const struct tm *time) {
//     uint32_t unix_time = 0;
//     uint16_t year = time->tm_year + 1900;

//     // Calculate seconds from date
//     // First calculate days from years since epoch
//     for (uint16_t y = EPOCH_YEAR; y < year; y++) {
//         unix_time += IS_LEAP_YEAR(y) ? DAYS_IN_LEAP_YEAR : DAYS_IN_YEAR;
//     }

//     // Add days from months
//     for (uint8_t m = 0; m < time->tm_mon; m++) {
//         unix_time += get_days_in_month(m, year);
//     }

//     // Add days in current month
//     unix_time += time->tm_mday - 1;

//     // Convert days to seconds and add time components
//     unix_time = unix_time * SECONDS_IN_DAY +
//                 time->tm_hour * SECONDS_IN_HOUR +
//                 time->tm_min * SECONDS_IN_MINUTE +
//                 time->tm_sec;

//     return unix_time;
// }

// static const char *month_names[] = {
//     "January", "February", "March", "April", "May", "June",
//     "July", "August", "September", "October", "November", "December"
// };

// static const char *day_names[] = {
//     "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
// };

// char *time_to_string(const struct time *tm) {
//     const char *month = month_names[tm->tm_mon];
//     const char *day_of_week = day_names[tm->tm_wday];

//     return format("%{type: str} %{type: str} %{type: uint}, %{type: uint} %{type: uint, precision: 2}:%{type: uint, precision: 2}:%{type: uint, precision: 2}",
//                   day_of_week, month, tm->tm_mday, tm->tm_year + EPOCH_YEAR, tm->tm_hour, tm->tm_min, tm->tm_sec);
// }


// // clock_frequency is in Hz
// static uint64_t clock_frequency = 0;

// void get_clock_frequency(void) {
//     asm volatile("mrs %0, cntfrq_el0" : "=r" (clock_frequency));
// }

// void sleep_s(uint32_t seconds) {
//     if (clock_frequency == 0) {
//         get_clock_frequency();
//     }

//     sleep_end = read_cntpct() + clock_frequency * seconds;
//     while (read_cntpct() < sleep_end)
//         ;
// }

// void sleep_ms(uint32_t milliseconds) {
//     if (clock_frequency == 0) {
//         get_clock_frequency();
//     }

//     sleep_end = read_cntpct() + clock_frequency * milliseconds / 1000;
//     while (read_cntpct() < sleep_end)
//         ;
// }

// void sleep_us(uint32_t microseconds) {
//     if (clock_frequency == 0) {
//         get_clock_frequency();
//     }

//     sleep_end = read_cntpct() + clock_frequency * microseconds / 1000000;
//     while (read_cntpct() < sleep_end)
//         ;
// }

// void sleep_ns(uint32_t nanoseconds) {
//     if (clock_frequency == 0) {
//         get_clock_frequency();
//     }

//     sleep_end = read_cntpct() + clock_frequency * nanoseconds / 1000000000;
//     while (read_cntpct() < sleep_end)
//         ;
// }

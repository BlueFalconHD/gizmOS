#include <stdint.h>
#include "time.h"
#include "device/rtc.h"
#include "string.h"
#include "memory.h"

// global variable that represents the time that sleep should end
static uint32_t sleep_end = 0;

// Array of days in each month (non-leap year)
static const uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// Helper function to get days in a specific month considering leap years
static uint8_t get_days_in_month(uint8_t month, uint16_t year) {
    if (month == 1 && IS_LEAP_YEAR(year)) { // February in leap year
        return 29;
    }
    return days_in_month[month];
}

void unix_time_to_tm(uint32_t unix_time, struct tm *tm) {
    uint32_t days, seconds_remaining, years;
    uint16_t year;
    uint8_t month;

    // Calculate days since epoch
    days = unix_time / SECONDS_IN_DAY;
    seconds_remaining = unix_time % SECONDS_IN_DAY;

    // Calculate time
    tm->tm_hour = seconds_remaining / SECONDS_IN_HOUR;
    seconds_remaining = seconds_remaining % SECONDS_IN_HOUR;
    tm->tm_min = seconds_remaining / SECONDS_IN_MINUTE;
    tm->tm_sec = seconds_remaining % SECONDS_IN_MINUTE;

    // Calculate year
    year = EPOCH_YEAR;
    years = days / 365;
    days -= years * 365;

    // Adjust for leap years
    for (uint16_t y = EPOCH_YEAR; y < EPOCH_YEAR + years; y++) {
        if (IS_LEAP_YEAR(y)) {
            days--;
        }
    }

    // Adjust year if we've gone too far
    while (days >= (IS_LEAP_YEAR(year) ? DAYS_IN_LEAP_YEAR : DAYS_IN_YEAR)) {
        days -= (IS_LEAP_YEAR(year) ? DAYS_IN_LEAP_YEAR : DAYS_IN_YEAR);
        year++;
    }

    tm->tm_year = year - 1900;
    tm->tm_yday = days;

    // Calculate month and day
    for (month = 0; month < 12; month++) {
        uint8_t dim = get_days_in_month(month, year);
        if (days < dim)
            break;
        days -= dim;
    }

    tm->tm_mon = month;
    tm->tm_mday = days + 1;

    // Calculate day of week (1/1/1970 was a Thursday)
    tm->tm_wday = (unix_time / SECONDS_IN_DAY + 4) % 7;

    // Set DST to 0 (not handled)
    tm->tm_isdst = 0;
}

uint32_t tm_to_unix_time(const struct tm *tm) {
    uint32_t unix_time = 0;
    uint16_t year = tm->tm_year + 1900;

    // Calculate seconds from date
    // First calculate days from years since epoch
    for (uint16_t y = EPOCH_YEAR; y < year; y++) {
        unix_time += IS_LEAP_YEAR(y) ? DAYS_IN_LEAP_YEAR : DAYS_IN_YEAR;
    }

    // Add days from months
    for (uint8_t m = 0; m < tm->tm_mon; m++) {
        unix_time += get_days_in_month(m, year);
    }

    // Add days in current month
    unix_time += tm->tm_mday - 1;

    // Convert days to seconds and add time components
    unix_time = unix_time * SECONDS_IN_DAY +
                tm->tm_hour * SECONDS_IN_HOUR +
                tm->tm_min * SECONDS_IN_MINUTE +
                tm->tm_sec;

    return unix_time;
}

// month names
static const char *month_names[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

// day names
static const char *day_names[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

void tm_to_string(const struct tm *tm, char *buf) {
    // year
    char year[5];
    itoa(tm->tm_year + 1900, year);

    // month
    const char *month = month_names[tm->tm_mon];

    // day
    char day[3];
    itoa(tm->tm_mday, day);

    // day of week
    const char *day_of_week = day_names[tm->tm_wday];

    // time
    char hour[3];
    itoa(tm->tm_hour, hour);
    char min[3];
    itoa(tm->tm_min, min);
    char sec[3];
    itoa(tm->tm_sec, sec);

    // day_of_week month day, year hour:min:sec
    gz_strcat(buf, day_of_week);
    gz_strcat(buf, " ");
    gz_strcat(buf, month);
    gz_strcat(buf, " ");
    gz_strcat(buf, day);
    gz_strcat(buf, ", ");
    gz_strcat(buf, year);
    gz_strcat(buf, " ");
    gz_strcat(buf, hour);
    gz_strcat(buf, ":");
    gz_strcat(buf, min);
    gz_strcat(buf, ":");
    gz_strcat(buf, sec);
}


// clock_frequency is in Hz
static uint64_t clock_frequency = 0;

void get_clock_frequency(void) {
    asm volatile("mrs %0, cntfrq_el0" : "=r" (clock_frequency));
}

void sleep_s(uint32_t seconds) {
    if (clock_frequency == 0) {
        get_clock_frequency();
    }

    sleep_end = read_cntpct() + clock_frequency * seconds;
    while (read_cntpct() < sleep_end)
        ;
}

void sleep_ms(uint32_t milliseconds) {
    if (clock_frequency == 0) {
        get_clock_frequency();
    }

    sleep_end = read_cntpct() + clock_frequency * milliseconds / 1000;
    while (read_cntpct() < sleep_end)
        ;
}

void sleep_us(uint32_t microseconds) {
    if (clock_frequency == 0) {
        get_clock_frequency();
    }

    sleep_end = read_cntpct() + clock_frequency * microseconds / 1000000;
    while (read_cntpct() < sleep_end)
        ;
}

void sleep_ns(uint32_t nanoseconds) {
    if (clock_frequency == 0) {
        get_clock_frequency();
    }

    sleep_end = read_cntpct() + clock_frequency * nanoseconds / 1000000000;
    while (read_cntpct() < sleep_end)
        ;
}

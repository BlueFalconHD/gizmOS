#ifndef TIME_H
#define TIME_H

#include <stdint.h>

#define SECONDS_IN_MINUTE 60
#define SECONDS_IN_HOUR 3600
#define SECONDS_IN_DAY 86400
#define MINUTES_IN_HOUR 60
#define MINUTES_IN_DAY 1440
#define HOURS_IN_DAY 24
#define DAYS_IN_YEAR 365
#define DAYS_IN_LEAP_YEAR 366

#define EPOCH_YEAR 1970

#define IS_LEAP_YEAR(year) ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)

struct tm {
    uint8_t tm_sec;   /* Seconds (0-59) */
    uint8_t tm_min;   /* Minutes (0-59) */
    uint8_t tm_hour;  /* Hours (0-23) */
    uint8_t tm_mday;  /* Day of the month (1-31) */
    uint8_t tm_mon;   /* Month (0-11) */
    uint16_t tm_year; /* Year - 1900 */
    uint8_t tm_wday;  /* Day of the week (0-6, Sunday = 0) */
    uint16_t tm_yday; /* Day of the year (0-365) */
    uint8_t tm_isdst; /* Daylight Saving Time */
};

// Convert Unix timestamp to tm structure
void unix_time_to_tm(uint32_t unix_time, struct tm *tm);

// Convert tm structure to Unix timestamp
uint32_t tm_to_unix_time(const struct tm *tm);

void tm_to_string(const struct tm *tm, char *buf);

#endif

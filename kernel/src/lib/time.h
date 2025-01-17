// #ifndef TIME_H
// #define TIME_H

// #include <stdint.h>

// #define SECONDS_IN_MINUTE 60
// #define SECONDS_IN_HOUR 3600
// #define SECONDS_IN_DAY 86400
// #define MINUTES_IN_HOUR 60
// #define MINUTES_IN_DAY 1440
// #define HOURS_IN_DAY 24
// #define DAYS_IN_YEAR 365
// #define DAYS_IN_LEAP_YEAR 366

// #define EPOCH_YEAR 1970

// #define IS_LEAP_YEAR(year) ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
// #define SECONDS_IN_YEAR(year) (IS_LEAP_YEAR(year) ? 31622400 : 31536000)


// enum WEEKDAY {
//     SUNDAY = 0,
//     MONDAY,
//     TUESDAY,
//     WEDNESDAY,
//     THURSDAY,
//     FRIDAY,
//     SATURDAY
// };

// enum MONTH {
//     JANUARY = 0,
//     FEBRUARY,
//     MARCH,
//     APRIL,
//     MAY,
//     JUNE,
//     JULY,
//     AUGUST,
//     SEPTEMBER,
//     OCTOBER,
//     NOVEMBER,
//     DECEMBER
// };

// typedef uint8_t hour_t;
// typedef uint8_t minute_t;
// typedef uint8_t second_t;
// typedef uint16_t year_t;
// typedef uint8_t day_t;

// typedef MONTH month_t;
// typedef WEEKDAY weekday_t;

// struct time {
//     second_t seconds;
//     minute_t minutes;
//     hour_t hours;
//     day_t day;
//     month_t month;
//     year_t year;
//     weekday_t weekday;
// };

// typedef struct time time_t;

// // Convert Unix timestamp to tm structure
// void unix_time_to_tm(uint32_t unix_time, struct tm *tm);

// // Convert tm structure to Unix timestamp
// uint32_t tm_to_unix_time(const struct tm *tm);

// void tm_to_string(const struct tm *tm, char *buf);

// void sleep_s(uint32_t seconds);
// void sleep_us(uint32_t microseconds);
// void sleep_ns(uint32_t nanoseconds);
// void sleep_ms(uint32_t milliseconds);

// /*
// TODO: impl time ser. - de.
// gizmOS Time Serialization Format:

// 1 byte magic: `0xd8`
// 1 byte metadata: `0xVF` where V is the version and F is the flags
//     Flags:
//     - 0b0001: Use long timestamp
//     - 0b0010: Timezone present
//     - 0b1100: Reserved
// 2 byte epoch year: Year of the epoch
// 4/8 byte timestamp: Unix timestamp
// 1 byte timezone: Timezone offset from UTC in hours
// */


// #endif

// #include "boot_time.h"
// #include "limine.h"
// #include "memory.h"
// #include "device/term.h"

// __attribute__((used, section(".limine_requests")))
// static volatile struct limine_boot_time_request boot_time_request = {
//     .id = LIMINE_BOOT_TIME_REQUEST,
//     .revision = 0
// };


// struct tm boot_time;

// void boot_time_init() {
//     struct limine_boot_time_response *boot_time_response = boot_time_request.response;
//     if (boot_time_response == NULL) {
//         print_error("Response to boot time request was null\n");
//     } else {
//         unix_time_to_tm(boot_time_response->boot_time, &boot_time);
//     }
// }

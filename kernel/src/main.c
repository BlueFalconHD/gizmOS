#include "device/framebuffer.h"
#include "device/rtc.h"
#include "device/term.h"
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"
#include "memory.h"
#include "string.h"
#include "time.h"
#include "dtb/dtb.h"

// Set the base revision to 3, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_boot_time_request boot_time_request = {
    .id = LIMINE_BOOT_TIME_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_dtb_request dtb_request = {
    .id = LIMINE_DTB_REQUEST,
    .revision = 0
};


// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;


// #define HEAP_SIZE 1024 * 1024 * 16 /* 16 MB heap size */

// uint8_t heap_area[HEAP_SIZE]; /* Heap memory */

static void hcf() {
    for (;;) {
        asm ("wfi");
    }
}

uint8_t triangle_oscillator(uint64_t time) {
    uint64_t period = 1000000000;
    uint64_t half_period = period / 2;
    uint64_t quarter_period = period / 4;
    uint64_t eighth_period = period / 8;

    uint64_t phase = time % period;

    if (phase < half_period) {
        return 0;
    } else if (phase < half_period + quarter_period) {
        return 1;
    } else if (phase < half_period + quarter_period + eighth_period) {
        return 0;
    } else {
        return 1;
    }
}


void kmain() {
    // Initialize the allocator with the heap area
    // memory_init(heap_area, HEAP_SIZE);

    // uint32_t fb_width = 640;
    // uint32_t fb_height = 360;
    // uint32_t fb_bpp = 4;  // Bytes per pixel
    // uint32_t fb_stride = fb_bpp * fb_width;

    // // Allocate memory for the framebuffer using ledger_malloc
    // void *fb_memory = malloc(fb_stride * fb_height);
    // if (!fb_memory) {
    //   uart_puts("ledger_malloc failed to allocate framebuffer memory\n");
    //   return;
    // }

    // if (check_fw_cfg_dma()) {
    //   uart_puts("guest fw_cfg dma-interface enabled \n");
    // } else {
    //   uart_puts("guest fw_cfg dma-interface not enabled \n");
    //   return;
    // }

    // fb_info fb = {
    //   .fb_addr = (uint64_t)fb_memory,  // Set framebuffer address to allocated memory
    //   .fb_width = fb_width,
    //   .fb_height = fb_height,
    //   .fb_bpp = fb_bpp,
    //   .fb_stride = fb_stride,
    //   .fb_size = fb_stride * fb_height,
    // };


    // /* since this is just a "proof of concept", the complete "heap" serves as framebuffer... */
    // if (ramfb_setup(&fb) != 0) {
    //   uart_puts("setup ramfb failed\n");
    // }

    // uart_puts("Framebuffer setup\n");

    // // uint8_t pixel[3] = {255, 255, 255};

    // // for (int i = 0; i < fb.fb_width; i++) {
    // //   for (int j = 0; j < fb.fb_height; j++) {
    // //     write_rgb256_pixel(&fb, i, j, pixel);
    // //   }
    // // }

    // uint8_t *rgb_map = (uint8_t *)fb_memory;

    // uint8_t black[3] = {0, 0, 0};

    // int line = 0;
    // int cursor_pos = 0;

    // // Buffer for user input
    // int size = 128;
    // int buffer_length = 0;
    // char buf[size];

    // while (1) {
    // }

    // free(fb_memory);

    // save content of x0 to variable
    uint64_t x0;
    asm volatile("mov %0, x0" : "=r"(x0));

    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }

    // Initialize the framebuffer
    struct limine_framebuffer *fb = get_framebuffer();
    term_init(fb);

    print_header("ahoy", "Welcome to gizmOS!\n");

    // Get boot time
    struct limine_boot_time_response *boot_time_response = boot_time_request.response;
    if (boot_time_response == NULL) {
        print_error("Response to boot time request to Limine was null\n");
    } else {
        print_header("time", "Booted at ");
        struct tm time;
        unix_time_to_tm(boot_time_response->boot_time,&time);
        char buffer[128];
        tm_to_string(&time, buffer);
        term_puts(buffer);
        term_puts("\n");
    }

    term_puts(ANSI_RESET);
    //term_puts("TODO: implement keyboard handling\n");

    // Get the device tree blob
    struct limine_dtb_response *dtb_response = dtb_request.response;
    if (dtb_response == NULL) {
        print_error("Response to DTB request to Limine was null\n");
    } else {
        // make fdt_header at the start of the dtb_ptr
        struct fdt_header *header = (struct fdt_header *)dtb_response->dtb_ptr;

        char buffer[128];
        itoa_hex(header->magic, buffer);

        print_header("dtb", " Magic number: 0x");
        term_puts(buffer);
        term_puts("\n");

        bool valid = verify_magic(header);

        if (!valid) {
            print_error("Invalid Device Tree Blob. Make sure virt.dtb is in img/\n");
            hcf();
        }

        print_header("dtb", " Version ");
        itoa(swap_uint32(header->version), buffer);
        term_puts(buffer);
        term_puts("\n");

        print_header("dtb", " Last compatible version ");
        itoa(swap_uint32(header->last_comp_version), buffer);
        term_puts(buffer);
        term_puts("\n");

        print_header("dtb", " Boot CPU ID ");
        itoa(swap_uint32(header->boot_cpuid_phys), buffer);
        term_puts(buffer);
        term_puts("\n");

        print_header("dtb", " Size of the Device Tree Blob ");
        itoa(swap_uint32(header->totalsize), buffer);
        term_puts(buffer);
        term_puts("\n");

    }

    print_header("done", "Finished all startup processes. TODO: keyboard input");

    // We're done, just hang...
    hcf();
}

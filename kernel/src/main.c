#include "device/framebuffer.h"
#include "device/rtc.h"
#include "device/term.h"
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"
#include "memory.h"
#include "memory_map.h"
#include "string.h"
#include "time.h"
#include "dtb/dtb.h"
#include "dtb/smoldtb/smoldtb.h"
#include "img/img.h"
#include "img/gizmOS_logo.h"
#include "physical_alloc.h"
#include "hhdm.h"


// Set the base revision to 3, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_paging_mode_request paging_mode_request = {
    .id = LIMINE_PAGING_MODE_REQUEST,
    .revision = 0,
    .mode = LIMINE_PAGING_MODE_AARCH64_4LVL
};




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

    // draw boot splash logo
    draw_image_aligned(fb, IMAGE_WIDTH, IMAGE_HEIGHT, color_palette, bitmap, IMAGE_ALIGN_HORIZONTAL_LEFT, IMAGE_ALIGN_VERTICAL_TOP);

    sleep_s(1);

    term_init(fb);



    print_header("BOOT", "Initializing kernel components.\n");

    char buffer[128];

    // Get boot time
    struct limine_boot_time_response *boot_time_response = boot_time_request.response;
    if (boot_time_response == NULL) {
        print_error("Response to boot time request was null\n");
    } else {
        term_puts("Booted at ");
        struct tm time;
        unix_time_to_tm(boot_time_response->boot_time,&time);
        tm_to_string(&time, buffer);
        term_puts(buffer);
        term_puts("\n");
    }

    term_puts(ANSI_RESET);

    print_header("DTB", "Fetching and parsing DTB\n");

    struct limine_dtb_response *dtb_response = dtb_request.response;
    if (dtb_response == NULL) {
        print_error("Response to DTB request to Limine was null\n");
    } else {
        init_dtb((uintptr_t)dtb_response->dtb_ptr);

        dtb_node *root = dtb_find("/");
        if (root == NULL) {
            print_error("Failed to find root node in DTB\n");
            hcf();
        }

        // get `intc`
        dtb_node *intc = dtb_find_child(root, "intc");
        if (intc == NULL) {
            print_error("Failed to find `intc` node in DTB\n");
            hcf();
        }


        dtb_prop* reg_prop = dtb_find_prop(intc, "reg");
        if (reg_prop != NULL) {
            size_t addr_cells = dtb_get_addr_cells_for(intc);
            uintmax_t address;
            if (dtb_read_prop_values(reg_prop, addr_cells, &address) > 0) {
                term_puts("Interrupt controller address: 0x");
                itoa_hex(address, buffer);
                term_puts(buffer);
                term_puts("\n");
            } else {
                print_error("Failed to read `reg` property\n");
                hcf();
            }
        }


    }

    hhdm_init();
    memory_map_init();


    if (paging_mode_request.response == NULL) {
        print_error("Response to paging mode request was null\n");
        hcf();
    } else {
        term_puts("Paging mode set to AArch64 4-level paging\n");

        // get response from paging mode request
        struct limine_paging_mode_response *paging_mode_response = paging_mode_request.response;

        // ensure mode value is correct
        if (paging_mode_response->mode != LIMINE_PAGING_MODE_AARCH64_4LVL) {
            print_error("Paging mode not set to AArch64 4-level paging\n");
            hcf();
        }
    }



    print_header("alloc", "Initializing pages\n");
    initialize_pages(memory_map_entries, memory_map_entry_count);


    uint64_t free_page_count = get_free_page_count();
    uint64_to_str(free_page_count, buffer);
    term_puts("Free pages: ");
    term_puts(buffer);
    term_puts("\n");



    // We're done, just hang...
    hcf();
}

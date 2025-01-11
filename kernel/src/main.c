#include "device/uart.h"
#include "font/font_render.h"
#include "memory.h"
#include "device/framebuffer.h"
#include "qemu/dma.h"
#include "device/rtc.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

// Set the base revision to 3, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
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

static void hcf(void) {
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

    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    // Note: we assume the framebuffer model is RGB with 32-bit pixels.
    for (size_t i = 0; i < 1000; i++) {
        volatile uint32_t *fb_ptr = framebuffer->address;
        fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
    }

    // We're done, just hang...
    hcf();
}

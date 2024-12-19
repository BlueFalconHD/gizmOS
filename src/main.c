#include "device/uart.h"
#include "font/font_render.h"
#include "memory.h"
#include "device/framebuffer.h"
#include "qemu/dma.h"
#include "device/rtc.h"
#include "device/rtc.h"

#define HEAP_SIZE 1024 * 1024 * 16 /* 16 MB heap size */

uint8_t heap_area[HEAP_SIZE]; /* Heap memory */

void main() {
    // Initialize the allocator with the heap area
    memory_init(heap_area, HEAP_SIZE);

    uint32_t fb_width = 256;
    uint32_t fb_height = 144;
    uint32_t fb_bpp = 4;  // Bytes per pixel
    uint32_t fb_stride = fb_bpp * fb_width;

    // Allocate memory for the framebuffer using ledger_malloc
    void *fb_memory = malloc(fb_stride * fb_height);
    if (!fb_memory) {
      uart_puts("ledger_malloc failed to allocate framebuffer memory\n");
      return;
    }

    if (check_fw_cfg_dma()) {
      uart_puts("guest fw_cfg dma-interface enabled \n");
    } else {
      uart_puts("guest fw_cfg dma-interface not enabled \n");
      return;
    }

    fb_info fb = {
      .fb_addr = (uint64_t)fb_memory,  // Set framebuffer address to allocated memory
      .fb_width = fb_width,
      .fb_height = fb_height,
      .fb_bpp = fb_bpp,
      .fb_stride = fb_stride,
      .fb_size = fb_stride * fb_height,
    };


    /* since this is just a "proof of concept", the complete "heap" serves as framebuffer... */
    if (ramfb_setup(&fb) != 0) {
      uart_puts("setup ramfb failed\n");
    }

    uart_puts("Framebuffer setup\n");

    // uint8_t pixel[3] = {255, 255, 255};

    // for (int i = 0; i < fb.fb_width; i++) {
    //   for (int j = 0; j < fb.fb_height; j++) {
    //     write_rgb256_pixel(&fb, i, j, pixel);
    //   }
    // }

    uint8_t *rgb_map = (uint8_t *)fb_memory;

    // draw font
    font_print_string(10, 10, "hello", &fb);

    while (1) {
    }

    free(fb_memory);
}

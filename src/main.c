#include "uart.h"
#include "memory.h"
#include "gear.h"
#include "command.h"
#include "string_utils.h"
#include "ledger_alloc/ledger.h"
#include "ledger_alloc/stress_test.h"
#include "graphics/fb.h"
#include "dma/qemu_dma.h"
#include "math/rand.h"
#include "math/trig.h"
#include "math/geo.h"
#include "device/rtc.h"

#define HEAP_SIZE 1024 * 1024 * 8 /* 1 MB heap size */

uint8_t heap_area[HEAP_SIZE]; /* Heap memory */

// void main(void)
// {
//     // uart_puts("Initializing root axel\n");

//     // unsigned int root_axel_gears_capacity = 1024; /* Adjust capacity as needed */
//     // unsigned int root_axel_size = sizeof(axel_t) + (root_axel_gears_capacity * sizeof(gear_t *));

//     // /* Initialize root axel */
//     // axel_t *root = (axel_t *)MEM_START;
//     // root->size = root_axel_gears_capacity;
//     // root->count = 0;

//     // /* Initialize mem_free to point after root axel and gears array */
//     // mem_free = MEM_START + root_axel_size;

//     // uart_puts("Initializing command history\n");

//     // /* Initialize the command history gear */
//     // init_command_history(root);

//     // uart_puts("Finished setup\n");
//     // uart_puts("gizmOS 0.0.1\n");
//     // uart_puts("gzsh repl\n");
//     // repl();


//     ledger_init(heap, HEAP_SIZE);

//     ledger_malloc(16);

// }

// is_in_circle returns 1 if the point (x, y) is inside the circle with center (cx, cy) and radius r
int is_in_circle(int x, int y, int cx, int cy, int r) {
    return (x - cx) * (x - cx) + (y - cy) * (y - cy) < r * r;
}

void main() {
    // Initialize the allocator with the heap area
    ledger_init(heap_area, HEAP_SIZE);

    init_rand();

    uint32_t fb_width = 256;
    uint32_t fb_height = 144;
    uint32_t fb_bpp = 4;  // Bytes per pixel
    uint32_t fb_stride = fb_bpp * fb_width;


    // Allocate memory for the framebuffer using ledger_malloc
    void *fb_memory = ledger_malloc(fb_stride * fb_height);
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

    while (1) {

        // Draw a rainbow 2D gradient constrained to a circle
        int cx = fb_width / 2;
        int cy = fb_height / 2;
        int radius = 50;

        for (int i = 0; i < fb_width; i++) {
            for (int j = 0; j < fb_height; j++) {
                uint8_t r, g, b;

                if (is_in_circle(i, j, cx, cy, radius)) {
                    // Map x coordinate to Hue
                    int hue = (i * 1536) / fb_width; // Hue from 0 to 1535

                    if (hue < 256) {             // Red to Yellow
                        r = 255;
                        g = hue;
                        b = 0;
                    } else if (hue < 512) {      // Yellow to Green
                        r = 511 - hue;
                        g = 255;
                        b = 0;
                    } else if (hue < 768) {      // Green to Cyan
                        r = 0;
                        g = 255;
                        b = hue - 512;
                    } else if (hue < 1024) {     // Cyan to Blue
                        r = 0;
                        g = 1023 - hue;
                        b = 255;
                    } else if (hue < 1280) {     // Blue to Magenta
                        r = hue - 1024;
                        g = 0;
                        b = 255;
                    } else {                     // Magenta to Red
                        r = 255;
                        g = 0;
                        b = 1535 - hue;
                    }

                    // Adjust brightness based on y
                    int brightness = (j * 255) / fb_height;
                    r = (r * brightness) / 255;
                    g = (g * brightness) / 255;
                    b = (b * brightness) / 255;
                } else {
                    // Outside circle, set pixel to black
                    r = 0;
                    g = 0;
                    b = 0;
                }

                rgb_map[(j * fb_stride) + (i * fb_bpp) + 0] = r; // R
                rgb_map[(j * fb_stride) + (i * fb_bpp) + 1] = g; // G
                rgb_map[(j * fb_stride) + (i * fb_bpp) + 2] = b; // B
            }
        }


        draw_rgb256_map(&fb, fb.fb_width, fb.fb_height, rgb_map);
    }

}

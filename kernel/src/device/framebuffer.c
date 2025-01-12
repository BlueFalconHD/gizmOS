#include "framebuffer.h"
#include "../memory.h"

static void hcf() {
    for (;;) {
        asm ("wfi");
    }
}


__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

struct limine_framebuffer *get_framebuffer(void) {
    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    return framebuffer;
}

void write_rgb256_pixel(struct limine_framebuffer *fb, uint32_t x, uint32_t y, uint8_t pixel[3]) {
    // volatile uint32_t *fb_ptr = framebuffer->address;
    // fb_ptr[y * (framebuffer->pitch / 4) + x] = color;

    volatile uint32_t *fb_ptr = fb->address;
    fb_ptr[y * (fb->pitch / 4) + x] = (0xFF << 24) | (pixel[0] << 16) | (pixel[1] << 8) | pixel[2];
}

void draw_rgb256_map(struct limine_framebuffer *fb, uint32_t x_res, uint32_t y_res, uint8_t *rgb_map) {
    for (uint32_t x = 0; x < x_res; x++) {
        for (uint32_t y = 0; y < y_res; y++) {
            uint8_t pixel[3] = {rgb_map[(y * x_res + x) * 3], rgb_map[(y * x_res + x) * 3 + 1], rgb_map[(y * x_res + x) * 3 + 2]};
            write_rgb256_pixel(fb, x, y, pixel);
        }
    }
}

#include "term.h"


#include "../limine.h"
#include "../flanterm/flanterm.h"
#include "../flanterm/backends/fb.h"
#include "../memory.h"
#include "../string.h"


static struct flanterm_context* ft_ctx = NULL;

void term_init(struct limine_framebuffer *fb) {
    ft_ctx = flanterm_fb_init(
        NULL,
        NULL,
        fb->address, fb->width, fb->height, fb->pitch,
        fb->red_mask_size, fb->red_mask_shift,
        fb->green_mask_size, fb->green_mask_shift,
        fb->blue_mask_size, fb->blue_mask_shift,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, 0, 0, 1,
        0, 0,
        0
    );
}

struct flanterm_context *get_ft_ctx() {
    return ft_ctx;
}

void term_putc(char c) {
    if (ft_ctx == NULL) {
        return;
    }

    flanterm_write(ft_ctx, &c, 1);
}

void term_puts(const char *s) {
    if (ft_ctx == NULL) {
        return;
    }

    flanterm_write(ft_ctx, s, strlen(s));
}

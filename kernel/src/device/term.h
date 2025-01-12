#ifndef TERM_H
#define TERM_H

#include "../limine.h"
#include "../flanterm/flanterm.h"
#include "../flanterm/backends/fb.h"

void term_init(struct limine_framebuffer *fb);

void term_putc(char c);
void term_puts(const char *s);


#endif // TERM_H

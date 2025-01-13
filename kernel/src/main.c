#include "boot_time.h"
#include "device/framebuffer.h"
#include "device/term.h"
#include <stdbool.h>
#include "limine.h"
#include "memory_map.h"
#include "paging_mode.h"
#include "time.h"
#include "dtb/dtb.h"
#include "img/img.h"
#include "img/gizmOS_logo.h"
#include "physical_alloc.h"
#include "hhdm.h"
#include "tests/physical_alloc_test.h"
#include "hcf.h"

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

void kmain() {
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }

    char buffer[128];

    struct limine_framebuffer *fb = get_framebuffer();
    draw_image_aligned(fb, IMAGE_WIDTH, IMAGE_HEIGHT, color_palette, bitmap, IMAGE_ALIGN_HORIZONTAL_CENTER, IMAGE_ALIGN_VERTICAL_CENTER);
    sleep_ms(250);

    term_init(fb);
    boot_time_init();
    hhdm_init();
    memory_map_init();
    dtb_init();
    paging_mode_init();
    initialize_pages(memory_map_entries, memory_map_entry_count);
    bool physical_alloc_test_results = run_physical_alloc_tests();
    if (!physical_alloc_test_results) {
        print_error("physical alloc tests failed");
        hcf();
    }


    hcf();
}

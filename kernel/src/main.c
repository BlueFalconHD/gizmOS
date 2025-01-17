#include "boot_time.h"
#include "device/framebuffer.h"
#include "device/term.h"
#include <stdbool.h>
#include <limine.h>
#include <memory_map.h>
#include <lib/str.h>
#include "paging_mode.h"
#include "dtb/dtb.h"
#include "physical_alloc.h"
#include "hhdm.h"
#include "tests/physical_alloc_test.h"
#include "hcf.h"
#include "lib/fmt.h"
#include <lib/panic.h>

#define VERSION "0.0.1"

// #define TESTS

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


    term_init(fb);
    // boot_time_init();
    hhdm_init();
    memory_map_init();
    dtb_init();
    paging_mode_init();
    initialize_pages(memory_map_entries, memory_map_entry_count);

    #ifdef TESTS
    bool physical_alloc_test_results = run_physical_alloc_tests();
    if (!physical_alloc_test_results) {
        print_error("physical alloc tests failed\n");
        hcf();
    }
    #endif


    term_puts("all components initialized\n");

    term_puts(format("Hello, %{type: uint}\n", "world!"));

    panic("hi");


}

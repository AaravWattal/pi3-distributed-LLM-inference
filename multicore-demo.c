#include "multicore.h"
#include "rpi.h"
#include "uart.h"

static volatile uint32_t core_results[4] __attribute__((aligned(8)));
static volatile uint32_t core_done[4] __attribute__((aligned(8)));

void multicore_main(uint32_t core_id) {
    // Do a simple calculation of core_id^2
    uint32_t result = core_id * core_id;
    core_results[core_id] = result;

    dev_barrier();

    core_done[core_id] = 1;
}

void run_multicore_demo(void) {
    printk("Starting multicore demo\r\n");

    // Clear results as sanity check
    for (int i = 0; i < 4; i++) {
        core_results[i] = 0;
        core_done[i] = 0;
    }

    // Start cores
    start_multicore(&multicore_main);

    // Have core0 do calculations
    core_results[0] = 0 * 0;

    dev_barrier();

    core_done[0] = 1;

    // Wait for other cores to finish
    while (!core_done[1] || !core_done[2] || !core_done[3]) {
        dev_barrier();
    }

    // Print results
    for (int i = 0; i < 4; i++) {
        printk("Core %d result: %d\r\n", i, core_results[i]);
    }
}
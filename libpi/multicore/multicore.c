#include "multicore.h"
#include "rpi.h"

#define ARM_BASE 0x40000000
#define CORE1_MBOX3_SET ARM_BASE + 0x9C
#define CORE2_MBOX3_SET ARM_BASE + 0xAC
#define CORE3_MBOX3_SET ARM_BASE + 0xBC

volatile uint32_t core_online[NUM_CORES];
volatile uint32_t cmd_funcs[NUM_CORES];
volatile uint32_t cmd_args[NUM_CORES];
volatile uint32_t cmd_seq[NUM_CORES];
volatile uint32_t cmd_done[NUM_CORES];
volatile int32_t cmd_results[NUM_CORES];

// Starts multicores through multicore_entry
void multicore_init() {
    // Write to core mailbox3 set registers
    PUT32(CORE1_MBOX3_SET, &multicore_entry);
    PUT32(CORE2_MBOX3_SET, &multicore_entry);
    PUT32(CORE3_MBOX3_SET, &multicore_entry);

    dev_barrier();
    asm volatile("sev");
}

// Signals other cores are online and ready to receive commands
void multicore_main(unsigned core_id) {
    // Set core as online
    core_online[core_id] = 1;
    dev_barrier();
    asm volatile("sev");

    command_loop(core_id);
}

// Waits for commands from other cores
void command_loop(unsigned core_id) {
    while (1) {
        // Wait for new command
        while (cmd_seq[core_id] == cmd_done[core_id]) {
            asm volatile("wfe");
        }

        uint32_t seq = cmd_seq[core_id];
        uint32_t func_addr = cmd_funcs[core_id];
        uint32_t arg = cmd_args[core_id];
        int32_t result = 0;

        // Execute function if it is not 0
        if (func_addr != 0) {
            int (*func)(void *) = (int (*)(void *))func_addr;
            result = func((void *)arg);
        }

        // Mark command as done
        cmd_results[core_id] = result;
        dev_barrier();
        cmd_done[core_id] = seq;
        dev_barrier();
        asm volatile("sev");
    }
}

int multicore_call(unsigned core_id, int (*func)(void*), void* arg, int* result) {
    // TODO
}
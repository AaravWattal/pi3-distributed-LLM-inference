#include "multicore.h"
#include "rpi.h"

#define ARM_BASE 0x40000000
#define CORE1_MBOX3_SET ARM_BASE + 0x9C
#define CORE2_MBOX3_SET ARM_BASE + 0xAC
#define CORE3_MBOX3_SET ARM_BASE + 0xBC

void start_multicore(void* entry_addr) {

    // Write to core mailbox3 set registers
    PUT32(CORE1_MBOX3_SET, entry_addr);
    PUT32(CORE2_MBOX3_SET, entry_addr);
    PUT32(CORE3_MBOX3_SET, entry_addr);

    dev_barrier();
    asm volatile("sev");
}
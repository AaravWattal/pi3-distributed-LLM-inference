#include "multicore.h"
#include "rpi.h"

#define SPIN_CPU1 0x000000E0
#define SPIN_CPU2 0x000000E8
#define SPIN_CPU3 0x000000F0

void start_multicore(void* entry_addr) {
    PUT32(SPIN_CPU1, entry_addr);
    PUT32(SPIN_CPU2, entry_addr);
    PUT32(SPIN_CPU3, entry_addr);

    // TODO not sure about this barrier
    dev_barrier();
}
#include "multicore.h"
#include "rpi.h"

static inline void dc_clean(volatile void *p) {
    asm volatile("mcr p15, 0, %0, c7, c10, 1" : : "r"(p) : "memory");
    dsb();
}

static inline void dc_inv(volatile void *p) {
    asm volatile("mcr p15, 0, %0, c7, c6, 1" : : "r"(p) : "memory");
    dsb();
}

#define ARM_BASE 0x40000000
#define CORE1_MBOX3_SET ARM_BASE + 0x9C
#define CORE2_MBOX3_SET ARM_BASE + 0xAC
#define CORE3_MBOX3_SET ARM_BASE + 0xBC

typedef struct { volatile uint32_t v; uint32_t _pad[15]; } cacheline_t;

cacheline_t core_online[NUM_CORES] __attribute__((aligned(64)));
cacheline_t cmd_funcs[NUM_CORES]   __attribute__((aligned(64)));
cacheline_t cmd_args[NUM_CORES]    __attribute__((aligned(64)));
cacheline_t cmd_seq[NUM_CORES]     __attribute__((aligned(64)));
cacheline_t cmd_done[NUM_CORES]    __attribute__((aligned(64)));

volatile int multicore_coherent = 0;

static int is_core_id_valid(unsigned core_id) {
    return core_id > 0 && core_id < NUM_CORES;
}

void multicore_init() {
    PUT32(CORE1_MBOX3_SET, &multicore_entry);
    PUT32(CORE2_MBOX3_SET, &multicore_entry);
    PUT32(CORE3_MBOX3_SET, &multicore_entry);

    dev_barrier();
    asm volatile("sev");

    // wait for everyone to get online
    while (core_online[1].v == 0 || core_online[2].v == 0 || core_online[3].v == 0) {
        dev_barrier();
        if (core_online[1].v != 0 && core_online[2].v != 0 && core_online[3].v != 0)
            break;
        asm volatile("wfe");
    }
}

// signals other cores are ready to receive commands, and then waits for commands
void multicore_main(unsigned core_id) {
    core_online[core_id].v = 1;
    dev_barrier();
    asm volatile("sev");

    while (1) {
        while (1) {
            dev_barrier();
            if (cmd_seq[core_id].v != cmd_done[core_id].v)
                break;
            asm volatile("wfe");
        }

        dev_barrier();
        uint32_t seq = cmd_seq[core_id].v;
        uint32_t func_addr = cmd_funcs[core_id].v;
        uint32_t arg = cmd_args[core_id].v;

        if (func_addr != 0) {
            multicore_worker_t func = (multicore_worker_t)func_addr;
            func((void *)arg);
        }

        dev_barrier();
        cmd_done[core_id].v = seq;
        dev_barrier();
        asm volatile("sev");
    }
}

int multicore_call(unsigned core_id, multicore_worker_t func, void* arg) {
    int status = multicore_call_async(core_id, func, arg);

    if (status != MULTICORE_OK) {
        return status;
    }

    return multicore_wait(core_id);
}

int multicore_call_async(unsigned core_id, multicore_worker_t func, void* arg) {
    if (!is_core_id_valid(core_id)) {
        return MULTICORE_ERR_CORE;
    }

    if (func == 0) {
        return MULTICORE_ERR_NULL;
    }

    dev_barrier();

    if (core_online[core_id].v == 0) {
        return MULTICORE_ERR_READY;
    }

    if (cmd_seq[core_id].v != cmd_done[core_id].v) {
        return MULTICORE_ERR_BUSY;
    }

    cmd_funcs[core_id].v = (uint32_t)func;
    cmd_args[core_id].v = (uint32_t)arg;
    dev_barrier();

    // Increment seq
    cmd_seq[core_id].v++;
    dev_barrier();

    if (!multicore_coherent) {
        dc_clean(&cmd_funcs[core_id].v);
        dc_clean(&cmd_args[core_id].v);
        dc_clean(&cmd_seq[core_id].v);
    } else {
        asm volatile("dsb sy" ::: "memory");
    }

    asm volatile("sev");

    return MULTICORE_OK;
}

// checks if command has finished
int multicore_check_done(unsigned core_id) {
    if (!is_core_id_valid(core_id)) {
        return MULTICORE_ERR_CORE;
    }

    dev_barrier();

    return cmd_seq[core_id].v == cmd_done[core_id].v;
}

int multicore_wait(unsigned core_id) {
    if (!is_core_id_valid(core_id)) {
        return MULTICORE_ERR_CORE;
    }

    dev_barrier();

    uint32_t expected_seq = cmd_seq[core_id].v;

    for (unsigned i = 0; ; i++) {
        if (!multicore_coherent) {
            dev_barrier();
            dc_inv(&cmd_done[core_id].v);
        } else {
            asm volatile("dsb sy" ::: "memory");
        }

        if (cmd_done[core_id].v == expected_seq) {
            break;
        }

        if (i > 50000000) {
            printk("HANG: core %d stuck: seq=%d done=%d func=0x%x arg=0x%x\n",
                   core_id, cmd_seq[core_id].v, cmd_done[core_id].v,
                   cmd_funcs[core_id].v, cmd_args[core_id].v);
            i = 0;
        }
    }

    return MULTICORE_OK;
}

void multicore_dcache_clean(void *addr, unsigned bytes) {
    uint32_t p   = (uint32_t)addr & ~(uint32_t)63;
    uint32_t end = (uint32_t)addr + bytes;
    for (; p < end; p += 64)
        asm volatile("mcr p15, 0, %0, c7, c10, 1" : : "r"(p) : "memory");
    dsb();
}

void multicore_dcache_clean_inv(void *addr, unsigned bytes) {
    uint32_t p   = (uint32_t)addr & ~(uint32_t)63;
    uint32_t end = (uint32_t)addr + bytes;
    for (; p < end; p += 64)
        asm volatile("mcr p15, 0, %0, c7, c14, 1" : : "r"(p) : "memory");
    dsb();
}

void multicore_dcache_inv(void *addr, unsigned bytes) {
    uint32_t p   = (uint32_t)addr & ~(uint32_t)63;
    uint32_t end = (uint32_t)addr + bytes;
    for (; p < end; p += 64)
        asm volatile("mcr p15, 0, %0, c7, c6, 1" : : "r"(p) : "memory");
    dsb();
}

int multicore_wait_all(void) {
    for (int i = 1; i < NUM_CORES; i++) {
        int status = multicore_wait(i);

        if (status != MULTICORE_OK) {
            return status;
        }
    }

    return MULTICORE_OK;
}

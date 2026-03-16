// main.c — entry point for Pi 3 bare-metal system.
//
// Toggle RUN_INFERENCE in the Makefile (or `make RUN_INFERENCE=1`)
// to switch between the cache performance demo and Llama inference.

#include "rpi.h"
#include "libc/demand.h"
#include "uart.h"

#if RUN_INFERENCE

// ============================================================
// Llama inference mode: delegate to run.c
// ============================================================
void notmain_llama_inference(void);

void notmain(void) {
    notmain_llama_inference();
}

#else // !RUN_INFERENCE

// ============================================================
// Cache performance demo (default)
// ============================================================
#include "pt-vm.h"
#include "memmap-default.h"
#include "procmap.h"

void fast_interrupt_vector(unsigned pc) { panic("FIQ at %x\n", pc); }
void interrupt_vector(unsigned pc)      { panic("IRQ at %x\n", pc); }
void reset_vector(unsigned pc)          { panic("RESET at %x\n", pc); }
void undefined_instruction_vector(unsigned pc) { panic("UNDEF at %x\n", pc); }
void syscall_vector(unsigned pc)        { panic("SWI at %x\n", pc); }
void prefetch_abort_vector(unsigned pc) { panic("PREFETCH ABORT at %x\n", pc); }
void data_abort_vector(unsigned pc)     { panic("DATA ABORT at %x\n", pc); }

static inline void enable_vfp(void) {
    unsigned r;
    asm volatile ("mrc p15, 0, %0, c1, c0, 2" : "=r"(r));
    r |= (0xF << 20);
    asm volatile ("mcr p15, 0, %0, c1, c0, 2" : : "r"(r));
    asm volatile ("dsb sy" ::: "memory");
    asm volatile ("isb sy" ::: "memory");
    asm volatile ("vmrs %0, fpexc" : "=r"(r));
    r |= 1 << 30;
    asm volatile ("vmsr fpexc, %0" : : "r"(r));
}

// --------------------------------------------------------
// ARMv7 Performance Monitor Unit (PMU) helpers.
//
// The Cortex-A53 uses the ARMv7 PMU (c9, c12/c13),
// NOT the ARM1176 PMU (c15, c12) from armv6-pmu.h.

enum {
    PMU_EVT_DCACHE_REFILL = 0x03,
    PMU_EVT_DCACHE_ACCESS = 0x04,
};

static inline void pmu_init(void) {
    uint32_t val;
    asm volatile ("mrc p15, 0, %0, c9, c12, 0" : "=r"(val));
    val |= (1 << 0)    // E: enable all counters
         | (1 << 1)    // P: reset event counters
         | (1 << 2)    // C: reset cycle counter
         | (1 << 3);   // D: cycle counter divider = 1 (every cycle)
    asm volatile ("mcr p15, 0, %0, c9, c12, 0" : : "r"(val));

    // enable cycle counter (bit 31) and event counter 0 (bit 0)
    asm volatile ("mcr p15, 0, %0, c9, c12, 1" : : "r"((1u << 31) | 1u));

    // select event counter 0
    asm volatile ("mcr p15, 0, %0, c9, c12, 5" : : "r"(0u));
    // program counter 0 to count L1 D-cache refills
    asm volatile ("mcr p15, 0, %0, c9, c13, 1" : : "r"((uint32_t)PMU_EVT_DCACHE_REFILL));
}

static inline uint32_t pmu_cycle_get(void) {
    uint32_t v;
    asm volatile ("mrc p15, 0, %0, c9, c13, 0" : "=r"(v));
    return v;
}

static inline uint32_t pmu_event0_get(void) {
    asm volatile ("mcr p15, 0, %0, c9, c12, 5" : : "r"(0u));
    uint32_t v;
    asm volatile ("mrc p15, 0, %0, c9, c13, 2" : "=r"(v));
    return v;
}

// --------------------------------------------------------

enum { OneMB = 1024 * 1024 };
enum { data_addr = OneMB * 10 };

void flush_caches(void);

static void caches_disable_all(void) {
    cp15_ctrl_reg1_t c = cp15_ctrl_reg1_rd();
    c.C_unified_enable = 0;
    c.W_write_buf = 0;
    c.L2_enabled = 0;
    c.Z_branch_pred = 0;
    c.I_icache_enable = 0;
    cp15_ctrl_reg1_wr(c);
    flush_caches();
}

static void caches_enable_all(void) {
    assert(mmu_is_enabled());
    cp15_ctrl_reg1_t c = cp15_ctrl_reg1_rd();
    c.C_unified_enable = 1;
    c.W_write_buf = 1;
    c.Z_branch_pred = 1;
    c.I_icache_enable = 1;
    cp15_ctrl_reg1_wr(c);
}

// --------------------------------------------------------

static volatile uint32_t sink;

static void array_sum(volatile uint32_t *arr, unsigned n,
                      unsigned stride)
{
    uint32_t sum = 0;
    for (unsigned i = 0; i < n; i += stride)
        sum += arr[i];
    sink = sum;
}

static void measure_array_sum(const char *msg,
                              volatile uint32_t *arr,
                              unsigned n, unsigned stride)
{
    unsigned miss0 = pmu_event0_get();
    unsigned cyc0  = pmu_cycle_get();

    array_sum(arr, n, stride);

    unsigned misses = pmu_event0_get() - miss0;
    unsigned cycles = pmu_cycle_get() - cyc0;

    printk("\t%s: cycles=%d, dcache misses=%d\n",
           msg, cycles, misses);
}

// --------------------------------------------------------

void notmain(void) {
    enable_vfp();
    uart_init();
    kmalloc_init_set_start((void*)OneMB, OneMB);

    printk("cache-test: starting ARMv7 cache demo on Pi 3\n");

    procmap_t p = procmap_default_mk(dom_kern);
    vm_pt_t *pt = vm_map_kernel(&p, 0);
    assert(!mmu_is_enabled());

    pin_t cached_attr = pin_mk_global(dom_kern, perm_rw_priv, MEM_wb_alloc);
    vm_map_sec(pt, data_addr, data_addr, cached_attr);

    mmu_sync_pte_mods();
    vm_mmu_enable();
    assert(mmu_is_enabled());
    printk("cache-test: MMU enabled\n");

    pmu_init();
    printk("cache-test: PMU initialized (counting dcache refills + cycles)\n");

    volatile uint32_t *arr = (volatile uint32_t *)data_addr;
    unsigned arr_n = 4096;

    for (unsigned i = 0; i < arr_n; i++)
        arr[i] = i;

    // ====================================================
    printk("========================================\n");
    printk("TEST 1: array sum (sequential, stride=1)\n");
    printk("========================================\n");

    caches_disable_all();
    measure_array_sum("no cache, stride=1, run 1", arr, arr_n, 1);
    measure_array_sum("no cache, stride=1, run 2", arr, arr_n, 1);

    caches_enable_all();
    flush_caches();
    measure_array_sum("cached, stride=1, run 1 (cold)", arr, arr_n, 1);
    measure_array_sum("cached, stride=1, run 2 (warm)", arr, arr_n, 1);
    measure_array_sum("cached, stride=1, run 3 (warm)", arr, arr_n, 1);

    // ====================================================
    printk("\n========================================\n");
    printk("TEST 2: array sum (strided, stride=64)\n");
    printk("========================================\n");

    caches_disable_all();
    measure_array_sum("no cache, stride=64, run 1", arr, arr_n, 64);
    measure_array_sum("no cache, stride=64, run 2", arr, arr_n, 64);

    caches_enable_all();
    flush_caches();
    measure_array_sum("cached, stride=64, run 1 (cold)", arr, arr_n, 64);
    measure_array_sum("cached, stride=64, run 2 (warm)", arr, arr_n, 64);

    // ====================================================
    printk("\n========================================\n");
    printk("done: compare cycle counts above!\n");
    printk("========================================\n");

    clean_reboot();
}

#endif // !RUN_INFERENCE

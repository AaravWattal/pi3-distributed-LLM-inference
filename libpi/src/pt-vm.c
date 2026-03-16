#include "rpi.h"
#include "asm-helpers.h"

#define PI3_MMIO_BASE 0x3F000000U

#define SECT_FLAG (1U << 1)
#define SECT_B (1U << 2)
#define SECT_C (1U << 3)
#define SECT_XN (1U << 4)
#define SECT_AP_FULL (3U << 10)
#define SECT_TEX(t) ((t) << 12)
#define SECT_S (1U << 16)

#define SECT_NORMAL (SECT_FLAG | SECT_TEX(1) | SECT_C | SECT_B | SECT_S | SECT_AP_FULL)

#define SECT_DEVICE (SECT_FLAG | SECT_AP_FULL | SECT_XN)

static unsigned pt[4096] __attribute__((aligned(1 << 14)));

static inline void icache_invalidate(void) {
    unsigned r = 0;
    asm volatile("mcr p15, 0, %0, c7, c5, 0" :: "r"(r) : "memory"); /* ICIALLU */
}

static inline void tlb_invalidate_all(void) {
    unsigned r = 0;
    asm volatile("mcr p15, 0, %0, c8, c7, 0" :: "r"(r) : "memory"); /* TLBIALL */
}

static void init_page_table(void) {
    for (unsigned i = 0; i < 4096; i++) {
        unsigned pa   = i << 20;
        unsigned attr;

        if (pa >= 0x40000000U) {
            attr = SECT_DEVICE;
        } else if (pa >= PI3_MMIO_BASE) {
            attr = SECT_DEVICE;
        } else {
            attr = SECT_NORMAL;
        }

        pt[i] = pa | attr;
    }

    dsb();
}

void mmu_enable(void) {
    unsigned r;

    asm volatile("mcr p15, 0, %0, c2, c0, 2" :: "r"(0U) : "memory");

    asm volatile("mcr p15, 0, %0, c2, c0, 0" :: "r"((unsigned)pt) : "memory");

    asm volatile("mcr p15, 0, %0, c3, c0, 0" :: "r"(1U) : "memory");

    asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(r));
    r &= ~(1U << 12);
    asm volatile("mcr p15, 0, %0, c1, c0, 0" :: "r"(r) : "memory");
    dsb();
    prefetch_flush();

    tlb_invalidate_all();
    icache_invalidate();
    dsb();
    prefetch_flush();

    asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(r));
    r |=  (1U << 0);
    r |=  (1U << 2);
    r |=  (1U << 12);
    r |=  (1U << 23);
    r &= ~(1U << 1);
    r &= ~(1U << 28);
    asm volatile("mcr p15, 0, %0, c1, c0, 0" :: "r"(r) : "memory");

    dsb();
    prefetch_flush();
}

void setup_vm(void) {
    printk("  init_page_table...\n");
    init_page_table();
    printk("  mmu_enable...\n");
    mmu_enable();
    printk("  mmu_enable done\n");
}

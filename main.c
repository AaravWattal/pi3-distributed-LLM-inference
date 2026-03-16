// Minimal VM demo for Pi 3 (Cortex-A53, ARMv7 AArch32).
// Sets up identity-mapped page table, enables MMU, performs
// a simple read/write test, then disables MMU.

#include "rpi.h"
#include "libc/demand.h"
#include "uart.h"
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

void notmain(void) {
    enable_vfp();
    uart_init();
    kmalloc_init_set_start((void*)(1024*1024), 1024*1024);

    printk("vm-test: starting ARMv7 VM demo on Pi 3\n");

    printk("vm-test: checking VM struct layouts\n");
    check_vm_structs();

    printk("vm-test: setting up domain\n");
    procmap_t m = procmap_default_mk(dom_kern);

    printk("vm-test: building page table (%d entries)\n", m.n);
    vm_pt_t *pt = vm_map_kernel(&m, 0);

    printk("vm-test: enabling MMU\n");
    mmu_sync_pte_mods();
    vm_mmu_enable();

    printk("vm-test: MMU ON -- running with virtual memory!\n");

    volatile uint32_t *p = (volatile uint32_t *)0x4000;
    *p = 0xdeadbeef;
    assert(*p == 0xdeadbeef);
    printk("vm-test: PASSED write/read at %p = 0x%x\n", p, *p);

    uint32_t pa = 0;
    vm_pte_t *pte = vm_xlate(&pa, pt, 0x4000);
    assert(pte);
    assert(pa == 0x4000);
    printk("vm-test: PASSED va 0x4000 -> pa 0x%x (identity map)\n", pa);

    vm_mmu_disable();
    printk("vm-test: MMU disabled successfully\n");

    printk("vm-test: ALL TESTS PASSED\n");
    clean_reboot();
}

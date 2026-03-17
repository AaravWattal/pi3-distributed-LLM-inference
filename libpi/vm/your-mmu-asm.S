@ armv7 MMU control assembly routines.

#include "rpi-asm.h"
#include "armv6-coprocessor-asm.h"


// just read the cache type: use C code to figure out if it is unified
@MK_FN(get_cache_type)
@    asm_not_implemented()
@    bx lr

// void reset(void)
// clear the dcache, icache, tlbs
MK_FN(mmu_reset)
    push {lr}

    CLR(r0)
    INV_TLB(r0)

    bl dcache_inv_by_sw

    CLR(r0)
    INV_ICACHE(r0)

    dsb sy
    isb sy

    pop {pc}

// void cp15_domain_ctrl_wr(uint32_t dom_reg);
// NOTE:
//   - definitely need to do prefetch flush 
//   - I don't *think* we need the full cp15 sync: flush_btb, 
//     dsb, prefetch flush.
@ MK_FN(cp15_domain_ctrl_wr)
@     b staff_cp15_domain_ctrl_wr
@     asm_not_implemented()
@     bx lr

// void cp15_set_procid_ttbr0(uint32_t proc_and_asid, fld_t *pt);
// armv7 b3.10.4
MK_FN(cp15_set_procid_ttbr0)
    push {r4, lr}

    dsb sy

    @ change ASID to 0
    mov r2, #0
    ASID_SET(r2)
    isb sy

    @ change TTBR
    TTBR_BASE_CTRL_WR(r2)
    isb sy

    @ zero TTBR1 (unused with N=0)
    TTBR1_SET(r2)

    @ encode TTBR0
    orr r4, r1, #0x09
    TTBR0_SET(r4)
    isb sy

    @ set ASID + procid
    dsb sy
    ASID_SET(r0)

    @ flush branch predictor and sync
    CLR(r2)
    FLUSH_BTB(r2)
    dsb sy
    isb sy

    pop {r4, pc}


// void mmu_disable_set_asm(cp15_ctrl_reg1_t c);
// armv7 b2.2.6, b2.2.9, 
MK_FN(mmu_disable_set_asm)
    push {r4, lr}
    mov r4, r0

    @ invalidate dcache
    bl dcache_clean_inv_by_sw
    dsb sy

    @ turn off MMU + caches
    bic r4, r4, #(7)
    CONTROL_REG1_WR(r4)
    isb sy

    @ invalidate icache
    CLR(r1)
    INV_ICACHE(r1)
    dsb sy

    @ flush branch predictor
    CLR(r1)
    FLUSH_BTB(r1)
    dsb sy
    isb sy

    pop {r4, pc}


// void mmu_enable_set_asm(cp15_ctrl_reg1_t c);
// armv7 b2.2.6, b2.2.9
MK_FN(mmu_enable_set_asm)
    push {lr}

    @ ensure all prior memory operations complete
    dsb sy

    @ invalidate entire TLB
    CLR(r1)
    INV_TLB(r1)
    dsb sy
    isb sy

    @ invalidate branch predictor
    CLR(r1)
    FLUSH_BTB(r1)
    dsb sy
    isb sy

    @ invalidate icache
    CLR(r1)
    INV_ICACHE(r1)
    dsb sy

    @ write sctlr with MMU enabled
    CONTROL_REG1_WR(r0)
    isb sy

    pop {pc}


MK_FN(mmu_sync_pte_mods)
    push {lr}

    @ clean + invalidate dcache so PTE writes are visible
    bl dcache_clean_inv_by_sw

    @ invalidate icache
    CLR(r0)
    INV_ICACHE(r0)
    dsb sy

    @ invalidate TLB
    CLR(r0)
    INV_TLB(r0)

    @ flush branch predictor
    CLR(r0)
    FLUSH_BTB(r0)
    dsb sy
    isb sy

    pop {pc}

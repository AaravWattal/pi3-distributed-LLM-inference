@ ARMv7 MMU control assembly routines.
@ Ported from ARMv6 (ARM1176JZF-S) to ARMv7 (Cortex-A53 AArch32).
@
@ Key changes from ARMv6:
@   - DSB/DMB/ISB are now dedicated instructions
@   - Whole-cache D-cache ops replaced with set/way function calls
@   - MMU enable sequence follows ARMv7 ARM §B3.10
@   - TTBR0 encoding includes IRGN bits for Multiprocessing Extensions
@   - TTBCR must have EAE=0 explicitly

#include "rpi-asm.h"
#include "armv6-coprocessor-asm.h"


@ just read the cache type: use C code to figure out if it is unified
MK_FN(get_cache_type)
    mrc p15, 0, r0, c0, c0, 1
    bx lr

@ void mmu_reset(void)
@ Invalidate TLB, D-cache, and I-cache.
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


@ void cp15_set_procid_ttbr0(uint32_t proc_and_asid, fld_t *pt)
@ Sequence from ARMv7 ARM §B3.10.4:
@   r0 = (procid << 8) | asid
@   r1 = page table base address (14-bit aligned)
MK_FN(cp15_set_procid_ttbr0)
    push {r4, lr}

    dsb sy

    @ zero ASID first to avoid TLB conflicts during switch
    mov r2, #0
    ASID_SET(r2)
    isb sy

    @ write TTBCR = 0 (EAE=0, N=0 — use only TTBR0)
    TTBR_BASE_CTRL_WR(r2)
    isb sy

    @ zero TTBR1 (unused with N=0)
    TTBR1_SET(r2)

    @ encode TTBR0: base | 0x09 for WB+WA inner/outer cacheable walks
    @ IRGN[1]=bit[0]=1, RGN=bits[4:3]=0b01 → 0x09
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


@ void mmu_disable_set_asm(cp15_ctrl_reg1_t c)
@ r0 = new SCTLR value with M=0
MK_FN(mmu_disable_set_asm)
    push {r4, lr}
    mov r4, r0                      @ save SCTLR value in callee-saved r4

    @ clean + invalidate D-cache before disabling MMU
    bl dcache_clean_inv_by_sw
    dsb sy

    @ turn off MMU + caches: clear bits [2:0] (M, A, C)
    bic r4, r4, #(7)
    CONTROL_REG1_WR(r4)
    isb sy

    @ invalidate I-cache
    CLR(r1)
    INV_ICACHE(r1)
    dsb sy

    @ flush branch predictor
    CLR(r1)
    FLUSH_BTB(r1)
    dsb sy
    isb sy

    pop {r4, pc}


@ void mmu_enable_set_asm(cp15_ctrl_reg1_t c)
@ r0 = new SCTLR value with M=1
@ Follows ARMv7 ARM §B3.10 required sequence.
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

    @ invalidate I-cache
    CLR(r1)
    INV_ICACHE(r1)
    dsb sy

    @ write SCTLR with MMU enabled
    CONTROL_REG1_WR(r0)
    isb sy                          @ required after MMU enable

    pop {pc}


@ void mmu_sync_pte_mods(void)
@ Called after modifying page table entries at runtime.
@ Ensures new mappings are visible to the TLB walker.
MK_FN(mmu_sync_pte_mods)
    push {lr}

    @ clean + invalidate D-cache so PTE writes are visible
    bl dcache_clean_inv_by_sw

    @ invalidate I-cache
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

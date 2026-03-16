// Hardware MMU code: mostly error checking, then calls into
// assembly (your-mmu-asm.S).
// Updated for ARMv7 (Cortex-A53 AArch32).
#include "asm-helpers.h"
#include "rpi.h"
#include "libc/demand.h"
#include "rpi-constants.h"
#include "mmu-internal.h"

cp_asm(cp15_ctrl, p15, 0, c1, c0, 0);
cp_asm(cp15_domain, p15, 0, c3, c0, 0);

void mmu_disable_set_asm(cp15_ctrl_reg1_t c);
void mmu_enable_set_asm(cp15_ctrl_reg1_t c);

int mmu_is_enabled(void) {
    return cp15_ctrl_reg1_rd().MMU_enabled != 0;
}

void mmu_disable_set(cp15_ctrl_reg1_t c) {
    assert(!c.MMU_enabled);

    uint32_t cache_on_p = c.C_unified_enable;
    mmu_disable_set_asm(c);

    if(cache_on_p) {
        c.C_unified_enable = 1;
        cp15_ctrl_reg1_wr(c);
    }
}

void mmu_disable(void) {
    cp15_ctrl_reg1_t c = cp15_ctrl_reg1_rd();
    assert(c.MMU_enabled);
    c.MMU_enabled=0;
    mmu_disable_set(c);
}

void mmu_enable_set(cp15_ctrl_reg1_t c) {
    assert(c.MMU_enabled);
    mmu_enable_set_asm(c);
}

void mmu_enable(void) {
    cp15_ctrl_reg1_t c = cp15_ctrl_reg1_rd();
    assert(!c.MMU_enabled);
    c.MMU_enabled = 1;
    mmu_enable_set(c);
}

// The assembly in cp15_set_procid_ttbr0 now handles the TTBR0
// encoding (ORing 0x09 for IRGN/RGN cache attributes).
void mmu_set_ctx(uint32_t pid, uint32_t asid, void *pt) {
    assert(asid!=0);
    assert(asid<64);
    cp15_set_procid_ttbr0(pid << 8 | asid, pt);
}

// ARMv7: SCTLR.XP_pt (bit[23]) is RAO/SBOP (always 1).
// We still set it for compatibility but it has no functional effect.
void mmu_init(void) { 
    mmu_reset();

    struct control_reg1 c1 = cp15_ctrl_reg1_rd();
    c1.XP_pt = 1;
    cp15_ctrl_reg1_wr(c1);

    c1 = cp15_ctrl_reg1_rd();
    assert(c1.XP_pt);
    assert(!c1.MMU_enabled);
}

uint32_t domain_access_ctrl_get(void) {
    return cp15_domain_get();
}

__attribute__((weak))
void domain_access_ctrl_set(uint32_t r) {
    cp15_domain_set(r);
    assert(domain_access_ctrl_get() == r);
}

cp15_ctrl_reg1_t cp15_ctrl_reg1_rd(void) {
    cp15_ctrl_reg1_t ret_val;
    asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(ret_val));
    return ret_val;
}

void cp15_ctrl_reg1_wr(cp15_ctrl_reg1_t c) {
    asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r"(c));
    // ARMv7: ISB after SCTLR write
    asm volatile("isb sy");
}

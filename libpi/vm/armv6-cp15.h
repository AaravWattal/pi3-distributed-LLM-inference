#ifndef __ARMV6_CP15_H__
#define __ARMV6_CP15_H__
// Data structures for ARM coprocessor registers.
// Updated for ARMv7 (Cortex-A53 AArch32).

/******************************************************************************
 * TLB configuration register (read-only).
 * ARMv7: implementation defined — Cortex-A53 may not match ARM1176.
 */
typedef struct tlb_config {
    unsigned
        unified_p:1,    // 0:1 0 = unified, 1 = separate I/D
        _sbz0:7,        // 1-7:7
        n_d_lock:8,     // 8-15:8
        n_i_lock:8,     // 16-23:8
        _sbz1:8;
} cp15_tlb_config_t;

_Static_assert(sizeof(cp15_tlb_config_t) == 4, "invalid size for struct tlb_config!");

cp15_tlb_config_t cp15_read_tlb_config(void);
void tlb_config_print(struct tlb_config *c);

/******************************************************************************
 * SCTLR — System Control Register (B4.1.130)
 *
 * Bit layout is compatible between ARMv6 and ARMv7 at the struct level,
 * but several fields changed semantics:
 *   - W_write_buf (bit[3]): ARMv7 = SWP disable, NOT write buffer enable
 *   - B_endian (bit[7]): RAZ/SBZP in ARMv7
 *   - S_prot (bit[8]): RAZ/SBZP in ARMv7
 *   - R_rom_prot (bit[9]): RAZ/SBZP in ARMv7
 *   - XP_pt (bit[23]): RAO/SBOP in ARMv7 (always reads 1)
 *   - Bits[4:3]: RAO/SBOP in ARMv7 (always read as 1)
 */
typedef struct control_reg1 {
    unsigned
        MMU_enabled:1,      // 0:1   0 = MMU disabled, 1 = enabled
        A_alignment:1,      // 1:1   0 = alignment check disabled
        C_unified_enable:1, // 2:1   L1 data cache enable
        W_write_buf:1,      // 3:1   ARMv7: SW bit (SWP disable), NOT write buffer
        _unused1:3,         // 4-6:3 RAO/SBOP in ARMv7 (bits 4:3 read as 1)
        B_endian:1,         // 7:1   RAZ/SBZP in ARMv7 (was big-endian in ARMv6)
        S_prot:1,           // 8:1   RAZ/SBZP in ARMv7
        R_rom_prot:1,       // 9:1   RAZ/SBZP in ARMv7
        F:1,                // 10:1  impl defined
        Z_branch_pred:1,    // 11:1  branch prediction enable
        I_icache_enable:1,  // 12:1  I-cache enable
        V_high_except_v:1,  // 13:1  high exception vectors
        RR_cache_rep:1,     // 14:1  cache replacement strategy
        L4:1,               // 15:1  ARM interworking inhibit
        _dt:1,              // 16:1  RAO/SBOP
        _sbz0:1,            // 17:1
        _it:1,              // 18:1  RAO/SBOP
        _sbz1:1,            // 19:1
        _st:1,              // 20:1
        F1:1,               // 21:1  fast interrupt
        U_unaligned:1,      // 22:1  unaligned access enable
        XP_pt:1,            // 23:1  RAO/SBOP in ARMv7 (always 1)
        VE_vect_int:1,      // 24:1  vectored interrupts
        EE:1,               // 25:1  exception endianness
        L2_enabled:1,       // 26:1  L2 cache (implementation specific)
        _reserved0:1,       // 27:1
        TR_tex_remap:1,     // 28:1  TEX remap enable
        FA_force_ap:1,      // 29:1  Access Flag Enable (AFE)
        _reserved1:2;       // 30-31:2
} cp15_ctrl_reg1_t;

cp15_ctrl_reg1_t cp15_ctrl_reg1_rd(void);
cp15_ctrl_reg1_t staff_cp15_ctrl_reg1_rd(void);

uint32_t cp15_ctrl_reg1_get(void);
void cp15_ctrl_reg1_wr(cp15_ctrl_reg1_t r);

/******************************************************************************
 * TTBR0/TTBR1 — Translation Table Base Registers.
 *
 * With Multiprocessing Extensions (Cortex-A53 has these), the low bits
 * encode inner/outer cacheability for page table walks (B4.1.154):
 *
 *   bit[6]    IRGN[0]
 *   bit[5]    NOS (Not Outer Shareable)
 *   bits[4:3] RGN[1:0] — outer cacheability
 *   bit[2]    IMP
 *   bit[1]    S — shareable
 *   bit[0]    IRGN[1]
 *
 * IRGN[1:0] encoding: 00=non-cacheable, 01=WB+WA, 10=WT, 11=WB no WA
 * RGN[1:0] encoding:  same as IRGN
 *
 * For WB+WA inner+outer walks: OR base with 0x09
 *   (IRGN[1]=bit[0]=1, RGN=bits[4:3]=0b01 → IRGN=0b01, RGN=0b01)
 *
 * We keep the struct simple — the assembly code handles encoding.
 */
typedef struct {
    unsigned base;
} cp15_tlb_reg_t;

cp15_tlb_reg_t cp15_ttbr0_rd(void);
void cp15_ttbr0_wr(cp15_tlb_reg_t r);

cp15_tlb_reg_t cp15_ttbr1_rd(void);
void cp15_ttbr1_wr(cp15_tlb_reg_t r);

// TTBCR: N field divides address range.  0 = just use ttbr0.
// ARMv7: bit[31] = EAE, MUST be 0 for short-descriptor format.
uint32_t cp15_ttbr_ctrl_rd(void);
void cp15_ttbr_ctrl_wr(uint32_t N);

struct first_level_descriptor;

uint32_t cp15_procid_rd(void);

void cp15_tlbr_print(void);
void cp15_domain_print(void);

#endif

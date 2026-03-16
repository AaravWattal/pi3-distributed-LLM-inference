#ifndef __ARMV6_CP15_H__
#define __ARMV6_CP15_H__
// engler, cs140e: the different data structures used by the arm coprocessor.

/******************************************************************************
 * TLB configuration register (read-only).
 */
typedef struct tlb_config {
    unsigned
        unified_p:1,    // 0:1 0 = unified, 1 = seperate I/D
        _sbz0:7,        // 1-7:7
        n_d_lock:8,     // 8-15:8  number of unified/data lockable entries
        n_i_lock:8,     //16-23:8 number of instruction lockable entries
        _sbz1:8;
} cp15_tlb_config_t;

_Static_assert(sizeof(cp15_tlb_config_t) == 4, "invalid size for struct tlb_config!");

cp15_tlb_config_t cp15_read_tlb_config(void);

// vm-helpers: print <c>
void tlb_config_print(struct tlb_config *c);

/******************************************************************************
 * System Control Register
 */
typedef struct control_reg1 {
    unsigned
        MMU_enabled:1,      // 0:1,   0 = MMU disabled, 1 = enabled,
        A_alignment:1,      // 1:1    0 = alignment check disabled.
        C_unified_enable:1, // 2:1    if unified used, this is enable/disable
        W_write_buf:1,      // 3:1    SW bit (SWP disable), not write buffer
        _unused1:3,         // 4:3
        B_endian:1,         // 7:1   RAZ/SBZP
        S_prot:1,           // 8:1   RAZ/SBZP
        R_rom_prot:1,       // 9:1   RAZ/SBZP
        F:1,                // 10:1  impl defined
        Z_branch_pred:1,    // 11:1  branch predict 0 = disabled, 1 = enabled
        I_icache_enable:1,  // 12:1  if seperate i/d, disables(0) or enables(1)
        V_high_except_v:1,  // 13:1  location of exception vec.  0 = normal
        RR_cache_rep:1,     // 14:1 cache replacement 0 = normal, 1 = different.
        L4:1,               // 15:1 inhibits arm internworking.
        _dt:1,              // 16:1, SBO
        _sbz0:1,            // 17:1 sbz
        _it:1,              // 18
        _sbz1:1,            // 19
        _st:1,              // 20
        F1:1,               // 21   fast interrupt.  [dunno]
        U_unaligned:1,      // 22 : unaligned
        XP_pt:1,            // 23:1  RAO/SBOP
        VE_vect_int:1,      // 24: no.  0
        EE:1,               // 25 endient exception a2-34 
        L2_enabled:1,       // 26,
        _reserved0:1,       // 27
        TR_tex_remap:1,     // 28
        FA_force_ap:1,      // 29  0 = disabled, 1 = force ap enabled      
        _reserved1:2; 
} cp15_ctrl_reg1_t;

cp15_ctrl_reg1_t cp15_ctrl_reg1_rd(void);
cp15_ctrl_reg1_t staff_cp15_ctrl_reg1_rd(void);

uint32_t cp15_ctrl_reg1_get(void);
void cp15_ctrl_reg1_wr(cp15_ctrl_reg1_t r);

/******************************************************************************
 * Translation Table Base Registers.
 */
typedef struct {
    unsigned base;
} cp15_tlb_reg_t;

cp15_tlb_reg_t cp15_ttbr0_rd(void);
void cp15_ttbr0_wr(cp15_tlb_reg_t r);

cp15_tlb_reg_t cp15_ttbr1_rd(void);
void cp15_ttbr1_wr(cp15_tlb_reg_t r);

// read the N that divides the address range.  0 = just use ttbr0
uint32_t cp15_ttbr_ctrl_rd(void);
// set the N that divides the address range b4-41
void cp15_ttbr_ctrl_wr(uint32_t N);

// must set both at once, AFAIK or hardware state too iffy.
struct first_level_descriptor;

uint32_t cp15_procid_rd(void);

void cp15_tlbr_print(void);
void cp15_domain_print(void);

#endif

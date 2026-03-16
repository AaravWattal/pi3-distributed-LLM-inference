#ifndef __ARM_COPROCESSOR_INSTS_H__
#define __ARM_COPROCESSOR_INSTS_H__
/*
 * Coprocessor instructions for ARMv7 (Cortex-A53 in AArch32 mode).
 * Ported from ARMv6 (ARM1176JZF-S).
 *
 * Key changes from ARMv6:
 *   - DSB/DMB/ISB are now dedicated instructions (not MCR encoding)
 *   - Whole-cache invalidate/clean ops (CLEAN_INV_DCACHE, INV_DCACHE)
 *     do not exist as single MCR instructions on ARMv7 — must use
 *     set/way loops (see cache-support.S)
 */

/************************************************************************
 * Barrier instructions — ARMv7 uses dedicated instructions, not CP15.
 * The (Rd) parameter is kept for source compatibility but ignored.
 */
#define DSB(Rd)             dsb sy
#define DMB(Rd)             dmb sy
#define PREFETCH_FLUSH(Rd)  isb sy

/* BPIALL: invalidate all branch predictor entries */
#define FLUSH_BTB(Rd)       mcr p15, 0, Rd, c7, c5, 6

/*
 * I-cache invalidate: ICIALLU.
 * ARMv7 does not have the ARMv6 repeat bug workaround, so one
 * ICIALLU followed by ISB is sufficient.
 */
#define INV_ICACHE(Rd)                              \
    mcr p15, 0, Rd, c7, c5, 0 ;  /* ICIALLU */    \
    isb sy

/*
 * ARMv7 does NOT support whole-cache clean/invalidate via a single MCR.
 * Use the dcache_clean_inv_by_sw / dcache_inv_by_sw routines in
 * cache-support.S which iterate by set/way.
 *
 * These macros are intentionally removed to produce a compile error
 * if old code tries to use them:
 *
 *   CLEAN_INV_DCACHE(Rd)  — removed
 *   INV_DCACHE(Rd)        — removed
 *   INV_ALL_CACHES(Rd)    — removed
 */

/*
 * TLB invalidation — encodings unchanged from ARMv6 (Table B3-50).
 */
#define INV_ITLB(Rd)        mcr p15, 0, Rd, c8, c5, 0
#define INV_DTLB(Rd)        mcr p15, 0, Rd, c8, c6, 0
/* invalidate unified TLB or both I/D TLB: TLBIALL */
#define INV_TLB(Rd)         mcr p15, 0, Rd, c8, c7, 0

/*
 * TTBR / TTBCR / ASID / Domain / Control register access.
 * Encodings unchanged from ARMv6.
 */
#define TTBR0_GET(Rd)           mrc p15, 0, Rd, c2, c0, 0
#define TTBR0_SET(Rd)           mcr p15, 0, Rd, c2, c0, 0
#define TTBR1_GET(Rd)           mrc p15, 0, Rd, c2, c0, 1
#define TTBR1_SET(Rd)           mcr p15, 0, Rd, c2, c0, 1
#define TTBR_BASE_CTRL_RD(Rd)   mrc p15, 0, Rd, c2, c0, 2
#define TTBR_BASE_CTRL_WR(Rd)   mcr p15, 0, Rd, c2, c0, 2

#define ASID_SET(Rd)            mcr p15, 0, Rd, c13, c0, 1
#define ASID_GET(Rd)            mrc p15, 0, Rd, c13, c0, 1

#define DOMAIN_CTRL_RD(Rd)      mrc p15, 0, Rd, c3, c0, 0
#define DOMAIN_CTRL_WR(Rd)      mcr p15, 0, Rd, c3, c0, 0

#define CACHE_TYPE_RD(Rd)       mrc p15, 0, Rd, c0, c0, 1
#define TLB_CONFIG_RD(Rd)       mrc p15, 0, Rd, c0, c0, 3

#define CONTROL_REG1_RD(Rd) mrc p15, 0, Rd, c1, c0, 0
#define CONTROL_REG1_WR(Rd) mcr p15, 0, Rd, c1, c0, 0

#define FAULT_STATUS_REG_GET(Rd) mrc p15, 0, Rd, c5, c0, 0

/* ARMv7 cache geometry registers used by set/way loops */
#define CSSELR_SET(Rd)      mcr p15, 2, Rd, c0, c0, 0
#define CCSIDR_GET(Rd)      mrc p15, 1, Rd, c0, c0, 0
/* DCISW: data cache invalidate by set/way */
#define DCISW(Rd)           mcr p15, 0, Rd, c7, c6, 2
/* DCCISW: data cache clean and invalidate by set/way */
#define DCCISW(Rd)          mcr p15, 0, Rd, c7, c14, 2

#endif

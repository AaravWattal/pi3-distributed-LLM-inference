#ifndef __ARM_COPROCESSOR_INSTS_H__
#define __ARM_COPROCESSOR_INSTS_H__
/*
 * Coprocessor instructions for armv7
 * Ported from armv6
 */

/************************************************************************
 * Barrier instructions — armv7 uses dedicated instructions
 */
#define DSB(Rd)             dsb sy
#define DMB(Rd)             dmb sy
#define PREFETCH_FLUSH(Rd)  isb sy

/* must do this after changing any MMU stuff, ASID, etc. */
#define FLUSH_BTB(Rd)       mcr p15, 0, Rd, c7, c5, 6

/*
 * I-cache invalidate
 */
#define INV_ICACHE(Rd)                              \
    mcr p15, 0, Rd, c7, c5, 0 ;      \
    isb sy

#define INV_ITLB(Rd)        mcr p15, 0, Rd, c8, c5, 0
#define INV_DTLB(Rd)        mcr p15, 0, Rd, c8, c6, 0
/* invalidate unified TLB or both I/D TLB */
#define INV_TLB(Rd)         mcr p15, 0, Rd, c8, c7, 0

/*
 * TTBR / TTBCR / ASID / Domain / Control register access
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

// cache geometry registers used by set/way loops
#define CSSELR_SET(Rd)      mcr p15, 2, Rd, c0, c0, 0
#define CCSIDR_GET(Rd)      mrc p15, 1, Rd, c0, c0, 0

// data cache invalidate by set/way
#define DCISW(Rd)           mcr p15, 0, Rd, c7, c6, 2

// data cache clean and invalidate by set/way
#define DCCISW(Rd)          mcr p15, 0, Rd, c7, c14, 2

#endif

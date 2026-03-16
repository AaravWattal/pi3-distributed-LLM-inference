#include "rpi-asm.h"
#include "armv6-coprocessor-asm.h"

@ ARMv7 cache maintenance routines.
@ Whole-cache clean/invalidate must iterate by set/way
@ since the single-MCR operations from ARMv6 do not exist.

@ -------------------------------------------------------
@ void dcache_inv_by_sw(void)
@ Invalidate entire L1 data cache by set/way (DCISW).
@ WARNING: discards dirty lines — only safe before D-cache
@ is enabled or when data loss is acceptable.
@ -------------------------------------------------------
MK_FN(dcache_inv_by_sw)
    push {r4-r7, lr}

    mov r0, #0
    mcr p15, 2, r0, c0, c0, 0      @ CSSELR = 0 (select L1 D-cache)
    isb sy

    mrc p15, 1, r0, c0, c0, 0      @ read CCSIDR into r0

    @ extract line size: log2(line_bytes) = (CCSIDR[2:0] + 2) + 2 = field + 4
    and r1, r0, #0x7
    add r1, r1, #4                  @ r1 = set shift (log2 line size in bytes)

    @ extract max way number: (CCSIDR[12:3])
    mov r2, r0, lsr #3
    ldr r3, =0x3FF
    and r2, r2, r3                  @ r2 = associativity - 1

    @ way shift = CLZ(associativity - 1)
    clz r3, r2                      @ r3 = way shift

    @ extract max set number: (CCSIDR[27:13])
    mov r4, r0, lsr #13
    ldr r5, =0x7FFF
    and r4, r4, r5                  @ r4 = num_sets - 1

    @ r1=set_shift, r2=max_way, r3=way_shift, r4=max_set
    @ iterate: way from max_way downto 0, set from max_set downto 0
    mov r5, r2                      @ r5 = way counter (saved copy of max_way)
1:  @ way loop
    mov r6, r4                      @ r6 = set counter = max_set
2:  @ set loop
    mov r7, r5, lsl r3             @ r7 = way << way_shift
    orr r7, r7, r6, lsl r1         @ r7 |= set << set_shift
    mcr p15, 0, r7, c7, c6, 2      @ DCISW
    subs r6, r6, #1
    bge 2b
    subs r5, r5, #1
    bge 1b

    dsb sy
    pop {r4-r7, pc}


@ -------------------------------------------------------
@ void dcache_clean_inv_by_sw(void)
@ Clean and invalidate entire L1 data cache by set/way (DCCISW).
@ -------------------------------------------------------
MK_FN(dcache_clean_inv_by_sw)
    push {r4-r7, lr}

    mov r0, #0
    mcr p15, 2, r0, c0, c0, 0      @ CSSELR = 0 (select L1 D-cache)
    isb sy

    mrc p15, 1, r0, c0, c0, 0      @ read CCSIDR

    and r1, r0, #0x7
    add r1, r1, #4                  @ r1 = set shift

    mov r2, r0, lsr #3
    ldr r3, =0x3FF
    and r2, r2, r3                  @ r2 = associativity - 1

    clz r3, r2                      @ r3 = way shift

    mov r4, r0, lsr #13
    ldr r5, =0x7FFF
    and r4, r4, r5                  @ r4 = num_sets - 1

    mov r5, r2                      @ r5 = way counter
1:  mov r6, r4                      @ r6 = set counter
2:  mov r7, r5, lsl r3             @ way << way_shift
    orr r7, r7, r6, lsl r1         @ | set << set_shift
    mcr p15, 0, r7, c7, c14, 2     @ DCCISW
    subs r6, r6, #1
    bge 2b
    subs r5, r5, #1
    bge 1b

    dsb sy
    pop {r4-r7, pc}


@ -------------------------------------------------------
@ void cache_flush_all(void)
@ void flush_caches(void)
@ Flush TLB, clean+inv D-cache, inv I-cache, flush BTB.
@ -------------------------------------------------------
MK_FN(cache_flush_all)
MK_FN(flush_caches)
    push {lr}

    CLR(r0)
    INV_TLB(r0)

    bl dcache_clean_inv_by_sw
    dsb sy

    CLR(r0)
    INV_ICACHE(r0)
    dsb sy

    CLR(r0)
    FLUSH_BTB(r0)
    isb sy

    pop {pc}

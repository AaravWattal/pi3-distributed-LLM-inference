#include "rpi-asm.h"
#include "armv6-coprocessor-asm.h"

@ armv7 cache maintenance routines.
@ entire-cache clean/invalidate must iterate by set/way
@ need this before when MMU is off
@ armv7 b2.2.6 + b2.2.7

@ -------------------------------------------------------
@ void dcache_inv_by_sw(void)
@ Invalidate entire L1 data cache by set/way
@ -------------------------------------------------------
MK_FN(dcache_inv_by_sw)
    push {r4-r7, lr}

    mov r0, #0
    mcr p15, 2, r0, c0, c0, 0      @ CSSELR = 0, select L1 D-cache
    isb sy

    mrc p15, 1, r0, c0, c0, 0

    @ extract line size
    and r1, r0, #0x7
    add r1, r1, #4

    @ extract max way number
    mov r2, r0, lsr #3
    ldr r3, =0x3FF
    and r2, r2, r3

    @ way shift
    clz r3, r2

    @ extract max set number
    mov r4, r0, lsr #13
    ldr r5, =0x7FFF
    and r4, r4, r5

    @ r1=set_shift, r2=max_way, r3=way_shift, r4=max_set
    @ iterate
    mov r5, r2
1:  @ way loop
    mov r6, r4
2:  @ set loop
    mov r7, r5, lsl r3
    orr r7, r7, r6, lsl r1 
    mcr p15, 0, r7, c7, c6, 2
    subs r6, r6, #1
    bge 2b
    subs r5, r5, #1
    bge 1b

    dsb sy
    pop {r4-r7, pc}


@ -------------------------------------------------------
@ void dcache_clean_inv_by_sw(void)
@ Clean and invalidate entire L1 data cache by set/way
@ -------------------------------------------------------
MK_FN(dcache_clean_inv_by_sw)
    push {r4-r7, lr}

    mov r0, #0
    mcr p15, 2, r0, c0, c0, 0      @ CSSELR = 0, select L1 D-cache
    isb sy

    mrc p15, 1, r0, c0, c0, 0

    and r1, r0, #0x7
    add r1, r1, #4

    mov r2, r0, lsr #3
    ldr r3, =0x3FF
    and r2, r2, r3

    clz r3, r2

    mov r4, r0, lsr #13
    ldr r5, =0x7FFF
    and r4, r4, r5

    mov r5, r2
1:  @ way loop
    mov r6, r4
2:  @ set loop
    mov r7, r5, lsl r3
    orr r7, r7, r6, lsl r1
    mcr p15, 0, r7, c7, c14, 2
    subs r6, r6, #1
    bge 2b
    subs r5, r5, #1
    bge 1b

    dsb sy
    pop {r4-r7, pc}


@ -------------------------------------------------------
@ void cache_flush_all(void)
@ void flush_caches(void)
@ Flush TLB, clean and inv D-cache, inv I-cache, flush BTB.
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
    dsb sy
    isb sy

    pop {pc}

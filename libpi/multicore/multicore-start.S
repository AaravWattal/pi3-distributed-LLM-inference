.global multicore_entry
multicore_entry:
    @ check in hyp mode
    @ if so, enable cache coherency
    mrs r1, cpsr
    and r1, r1, #0x1f
    cmp r1, #0x1a              @ HYP mode?
    bne set_core_stack         @ already in SVC, skip

    @ Enable hardware coherency
    @ arm a53-cortex docs 4.5.77
    mrrc p15, 1, r2, r3, c15
    orr r2, r2, #(1 << 6)
    mcrr p15, 1, r2, r3, c15
    isb

    @ Clear HCR
    mov r1, #0
    mcr p15, 4, r1, c1, c1, 0

    @ Drop to SVC mode with IRQs disabled
    mov r1, #(0x13 | (1 << 7))  @ SVC mode | IRQ disabled
    msr spsr_hyp, r1
    adr r1, set_core_stack
    msr elr_hyp, r1
    eret

set_core_stack:
    @ read core id from mpidr
    mrc p15, 0, r0, c0, c0, 5
    and r0, r0, #3

    cmp r0, #1
    beq core1_stack
    cmp r0, #2
    beq core2_stack
    cmp r0, #3
    beq core3_stack

    @ in case core 0 ends up here
    ldr r1, =0x80000
    b set_stack

core1_stack:
    ldr r1, =0x90000
    b set_stack
core2_stack:
    ldr r1, =0xA0000
    b set_stack
core3_stack:
    ldr r1, =0xB0000
    b set_stack

set_stack:
    mov sp, r1

    bl multicore_main

multicore_hang:
    wfe
    b multicore_hang
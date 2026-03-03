.global multicore_entry
multicore_entry:
    @ read core id from mpidr
    mrc p15, 0, r0, c0, c0, 5
    and r0, r0, #3

    @ set core stack
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
    ldr r1, =0x81000
    b set_stack
core2_stack:
    ldr r1, =0x82000
    b set_stack
core3_stack:
    ldr r1, =0x83000
    b set_stack

set_stack:
    mov sp, r1

    bl multicore_main

multicore_hang:
    wfe
    b multicore_hang
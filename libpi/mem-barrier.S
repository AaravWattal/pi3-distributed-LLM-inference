@ call to sync across different broadcom devices.
.globl dev_barrier
dev_barrier:
    b dsb

@ Data Memory Barrier (DMB): see ch B2 (esp B2-18 --- B2-25) in
@ the armv6 manual.
.globl dmb
dmb:
    dmb sy
    bx lr

@ DSB: used to be called "drain write buffer"
@ Includes all cache operations.  is a superset (>) DMB
@ See ch b2.
.globl dsb
dsb:
    dsb sy
    bx lr

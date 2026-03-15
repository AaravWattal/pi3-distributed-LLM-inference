#include "rpi-asm.h"

@ linker hack so we can force this file in the first portion of 
@ the linked binary.
.section ".text.boot"

@ this is where we start.  if you look in any '.list' file, you 
@ better see code at the first addresses (in our case: starting
@ at 0x8000).
.globl _start
_start:
    @ Need to be in SVC mode for IRQ exceptions
    @ Cannot be in HYP
    mrs r0, cpsr
    and r1, r0, #0x1f
    cmp r1, #0x1a              @ 0x1a = HYP_MODE
    bne svc

    @ Clear HCR so IRQs are not trapped
    mov r0, #0
    mcr p15, 4, r0, c1, c1, 0

    @ Set SPSR_hyp
    @ Disable interrupts
    mov r0, #(SUPER_MODE | (1<<7))
    msr spsr_hyp, r0

    @ Leave HYP and enter SVC
    adr r0, svc
    msr elr_hyp, r0
    eret

svc:
    mov sp, #STACK_ADDR @ initialize stack pointer
    mov fp, #0          @ clear frame pointer reg.  don't think needed.
    @ bl notmain        @ we could jump right to notmain
    bl _cstart          @ call our code to do initialization.
    bl rpi_reboot       @ if they return just reboot.

@ keep put32/get32 here so that they are in the same location in every binary.
@ if you want to override them easily, can re-define the name in rpi-asm.h
@
@ we write them as assembly routines to make absolutely sure the compiler
@ cannot optimize them (reorder, eliminate, cache, etc.)

@
@ put32(addr,val).  c-code equivalant:
@   - put32(addr, val) { *(uint32_t*)addr = val; }
@
@ store value <val> (passed in second argument register r1) 
@ into memory address <addr> (passed in first argument 
@ register, r0).
@
.globl put32
.globl PUT32
put32:
PUT32:
    str r1,[r0]     @ store r1 into address held in r0
    bx lr           @ return

@ get32(addr)
@
@ c-code equivalent: 
@   - get32(addr) { return *(uint32_t*)addr; }
@
@ load value of <addr> passed in first argument register (r0).
@
.globl get32
.globl GET32
get32:
GET32:
    ldr r0,[r0]     @ load address held in r0 into r0
    bx lr           @ return


@ jump to the address in r0.  we don't mess w/ link register
@ so called routine will return to our caller.
.globl BRANCHTO
BRANCHTO:
    bx r0

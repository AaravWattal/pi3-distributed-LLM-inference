#include "rpi-asm.h"

@ linker hack so we can force this file in the first portion of 
@ the linked binary.
.section ".text.boot"

@ this is the whole purpose for this <start.S>: so we can 
@ have a gap at the start so we can copy the sent code and
@ not have it hit the bootloader code.
.globl _start
_start:
    b skip
.space 0x200000-0x8004,0
skip:
    mov sp,#0x08000000
    bl _cstart
hang: b rpi_reboot

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


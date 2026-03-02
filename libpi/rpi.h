/*
 * General functions we use.  These could be broken into multiple small
 * header files, but that's kind of annoying to context-switch through,
 * so we put all the main ones here.
 */
#ifndef __RPI_H__
#define __RPI_H__

#define RPI_COMPILED

#include <stdint.h>

/*****************************************************************************
 * output routines.
 */


// let the user override the system putchar routine.

typedef int (*rpi_putchar_t)(int chr);
// put a single char
extern rpi_putchar_t rpi_putchar;

// override the routine.
rpi_putchar_t rpi_putchar_set(rpi_putchar_t putc);
#define rpi_set_putc rpi_putchar_set

// print string to uart (via rpi_putchar)
int putk(const char *p);

/***************************************************************************
 * simple timer functions.
 */

// delays for <ticks> (each tick = a few cycles)
void delay_cycles(uint32_t ticks) ;

// delay for <us> microseconds.
void delay_us(uint32_t us) ;

// delay for <ms> milliseconds
void delay_ms(uint32_t ms) ;

// returns time in usec.
// NOTE: this can wrap around!   do not do direct comparisons.
// this does a memory barrier.
uint32_t timer_get_usec(void) ;

// no memory barrier.
uint32_t timer_get_usec_raw(void);

/****************************************************************************
 * Reboot the pi smoothly.
 */

// reboot the pi.
void rpi_reboot(void) __attribute__((noreturn));

// reboot after printing out a string to cause the unix my-install to shut down.
void clean_reboot(void) __attribute__((noreturn));

// user can provide an implementation: will get called during reboot.
void reboot_callout(void);

/*****************************************************************************
 * memory related helpers
 */

// memory barrier.
void dmb(void);
// sort-of write memory barrier (more thorough).  dsb() >> dmb().
void dsb(void);
// use this if you need a device memory barrier.
void dev_barrier(void);

/*****************************************************************************
 * Low-level code: you could do in C, but these are in assembly to defeat
 * the compiler.
 */
// *(unsigned *)addr = v;
void PUT32(unsigned addr, unsigned v);
void put32(volatile void *addr, unsigned v);

// *(unsigned *)addr
unsigned GET32(unsigned addr);
unsigned get32(const volatile void *addr);

// *(volatile uint8_t *)addr = x;
void put8(volatile void *addr, uint8_t x);
void PUT8(uint32_t addr, uint8_t x);

uint8_t GET8(unsigned addr);
uint8_t get8(const volatile void *addr);

// jump to <addr>
void BRANCHTO(unsigned addr);

// a no-op routine called to defeat the compiler.
void dummy(unsigned);
void nop(void);

void rpi_wait(void);

/*********************************************************
 * some gcc helpers.
 */

// gcc memory barrier.
#define gcc_mb() asm volatile ("" : : : "memory")

#endif

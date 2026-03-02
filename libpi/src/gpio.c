/*
 * Implement the following routines to set GPIO pins to input or 
 * output, and to read (input) and write (output) them.
 *  1. DO NOT USE loads and stores directly: only use GET32 and 
 *    PUT32 to read and write memory.  See <start.S> for thier
 *    definitions.
 *  2. DO USE the minimum number of such calls.
 * (Both of these matter for the next lab.)
 *
 * See <rpi.h> in this directory for the definitions.
 *  - we use <gpio_panic> to try to catch errors.  For lab 2
 *    it only infinite loops since we don't have <printk>
 */
#include "rpi.h"
#include "gpio.h"

// See broadcomm documents for magic addresses and magic values.
//
// If you pass addresses as:
//  - pointers use put32/get32.
//  - integers: use PUT32/GET32.
//  semantics are the same.
enum {
    // Max gpio pin number.
    GPIO_MAX_PIN = 53,

    GPIO_BASE = 0x3F200000,
    gpio_set0  = (GPIO_BASE + 0x1C),
    gpio_clr0  = (GPIO_BASE + 0x28),
    gpio_lev0  = (GPIO_BASE + 0x34)

    // <you will need other values from BCM2835!>
};

//
// Part 1 implement gpio_set_on, gpio_set_off, gpio_set_output
//

// set <pin> to be an output pin.
//
// NOTE: fsel0, fsel1, fsel2 are contiguous in memory, so you
// can (and should) use ptr calculations versus if-statements!
void gpio_set_output(unsigned pin) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);

    unsigned int* addr = (unsigned int*)GPIO_BASE + (pin / 10);

    unsigned value = get32(addr);

    unsigned mask = 0x7 << (3 * (pin % 10));

    value &= ~mask;
    value |= 0x1 << (3 * (pin % 10));

    put32(addr, value);
}

// Set GPIO <pin> = on.
void gpio_set_on(unsigned pin) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);

    unsigned int* addr = (unsigned int*)gpio_set0 + (pin / 32);

    unsigned value = 0x1 << (pin % 32);

    put32(addr, value);
}

// Set GPIO <pin> = off
void gpio_set_off(unsigned pin) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);

    unsigned int* addr = (unsigned int*)gpio_clr0 + (pin / 32);

    unsigned value = 0x1 << (pin % 32);

    put32(addr, value);
}

// Set <pin> to <v> (v \in {0,1})
void gpio_write(unsigned pin, unsigned v) {
    if(v)
        gpio_set_on(pin);
    else
        gpio_set_off(pin);
}

//
// Part 2: implement gpio_set_input and gpio_read
//

// set <pin> = input.
void gpio_set_input(unsigned pin) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);

    unsigned int* addr = (unsigned int*)GPIO_BASE + (pin / 10);

    unsigned value = get32(addr);

    unsigned mask = 0x7 << (3 * (pin % 10));

    value &= ~mask;

    put32(addr, value);
}

// Return 1 if <pin> is on, 0 if not.
int gpio_read(unsigned pin) {
    unsigned v = 0;

    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);

    unsigned int* addr = (unsigned int*)gpio_lev0 + (pin / 32);

    v = get32(addr);

    unsigned mask = 0x1 << (pin % 32);

    v = (v & mask) >> (pin % 32);

    return v;
}

void gpio_set_function(unsigned pin, gpio_func_t func) {
    if(pin > GPIO_MAX_PIN)
        gpio_panic("illegal pin=%d\n", pin);
    if((func & 0b111) != func)
        // NOTE: it says "func" not "function" and uses "%x" not "%d"
        gpio_panic("illegal func=%x\n", func); 

    unsigned int* addr = (unsigned int*)GPIO_BASE + (pin / 10);

    unsigned value = get32(addr);

    unsigned mask = 0x7 << (3 * (pin % 10));

    value &= ~mask;
    value |= func << (3 * (pin % 10));

    put32(addr, value);
}

// weird panic: we just infinite loop since we don't have 
// printk in 2-gpio.
void panic(const char *msg, ...) {
    (void)msg;
    while(1);
}

#ifndef __MEMMAP_DEFAULT_H__
#define __MEMMAP_DEFAULT_H__

#include "rpi-constants.h"
#include "memmap.h"

#define MB(x) ((x)*1024*1024)

// default domains for kernel and user.
enum {
    dom_kern = 1,
    dom_user = 2,

    dom_bits = DOM_client << (dom_kern*2) 
        | DOM_client << (dom_user*2) 
};

enum { 
    no_user = perm_rw_priv,
    user_access = perm_rw_user,
};

enum {
    default_ASID = 1
};

// Default segment addresses for Pi 3 (Cortex-A53).
enum {
    SEG_CODE = MB(0),
    SEG_HEAP = MB(1),
    SEG_STACK = STACK_ADDR - MB(1),
    SEG_INT_STACK = INT_STACK_ADDR - MB(1),

    // Pi 3 BCM2837 peripherals at 0x3F000000 (was 0x20000000 on Pi Zero)
    SEG_BCM_0 = 0x3F000000,
    SEG_BCM_1 = SEG_BCM_0 + MB(1),
    SEG_BCM_2 = SEG_BCM_0 + MB(2),

    // Pi 3 ARM local peripherals (timer, mailboxes, etc.)
    SEG_LOCAL = 0x40000000,

    SEG_ILLEGAL = MB(2),
};

static inline pin_t dev_attr_default(void) {
    return pin_mk_global(dom_kern, no_user, MEM_device);
}
static inline pin_t kern_attr_default(void) {
    return pin_mk_global(dom_kern, no_user, MEM_uncached);
}

#endif

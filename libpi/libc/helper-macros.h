#ifndef __HELPER_MACROS_H__
#define __HELPER_MACROS_H__

#include <stddef.h>

// check that a bitfield <field> in struct <T> starts at bit
// offset <off> and has <nbits> bits.
#define check_bitfield(T, field, off, nbits) do {               \
    union { T s; unsigned u; } _x = { .u = 0 };                \
    unsigned _n = nbits;                                        \
    /* set the field to all 1s */                               \
    _x.s.field = (1U << _n) - 1;                               \
    /* check the mask is at the right offset */                 \
    unsigned _expected = ((1U << _n) - 1) << (off);             \
    assert(_x.u == _expected);                                  \
} while(0)

#define print_field(x, field) do {                              \
    printk("\t0b%b\t= %s\n", (x)->field, #field);              \
    if(((x)->field) > 8)                                        \
        printk("\t%x\n", (x)->field);                           \
} while(0)

#endif

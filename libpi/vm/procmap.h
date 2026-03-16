#ifndef __PROC_MAP_H__
#define __PROC_MAP_H__
#include "pinned-vm.h"
#include "memmap-default.h"


// physical address of our kernel: used to map.
// probably should tag with the domain?
// i think just use the page table entry itself.  give
// some simple routines to make cached, uncached, device
// memory, readonly, etc
typedef struct {
    uint32_t addr, nbytes;
    // need to have privileged.
    enum { MEM_DEVICE, MEM_RW, MEM_RO } type;
    unsigned dom;
} pr_ent_t;
static inline pr_ent_t
pr_ent_mk(uint32_t addr, uint32_t nbytes, int type, unsigned dom) {
    demand(nbytes == 1024*1024, only doing sections first);
    demand(dom < 16, "illegal domain %d\n", dom);
    return (pr_ent_t) {
        .addr = addr,
        .nbytes = nbytes,
        .type = type,
        .dom = dom
    };
}

typedef struct {
#   define MAX_ENT 16
    unsigned n;
    unsigned dom_ids;       // all the domain ids in use.
    pr_ent_t map[MAX_ENT];
} procmap_t;
// add entry <e> to procmap <p>
static inline void procmap_push(procmap_t *p, pr_ent_t e) {
    assert(p->n < MAX_ENT);
    assert(e.dom < 16);
    p->dom_ids |= 1 << e.dom;
    p->map[p->n++] = e;
}

// given a bitvector of which domains are in use,
// compute the domain permission bits to use
static inline uint32_t 
dom_perm(uint32_t doms, unsigned perm) {
    assert(doms);
    assert(doms >> 16 == 0);
    // [this is manager: not right]
    assert(perm <= 0b11);
    unsigned mask = 0;
    for(unsigned i = 0; i < 16; i++) {
        if(doms & (1 <<  i))
            mask |= perm << i*2;
    }
    return mask;
}

static inline pr_ent_t *
procmap_lookup(procmap_t *pmap, const void *addr) {
    for(int i = 0; i < pmap->n; i++) {
        pr_ent_t *p = &pmap->map[i];

        void *s = (void*)p->addr;
        void *e = s + p->nbytes;
        if(addr >= s && addr < e)
            return p;
    }
    return 0;
}

// memory map of address space.  each chunk of memory should have an entry.
// must be aligned to the size of the memory region.
//
// we don't seem able to search in the TLB if MMU is off.  
// 
// limitations in overall scheme:
//   - currently just tag everything with the same domain.
//   - should actually track what sections are allocated.
//   - basic map of our address space: should add sanity checking that ranges
//     are not overlapping.   
//   - our overall vm scheme is janky since still has assumptions encoded.
//   - in real life would want more control over region attributes.
//
// mapping static sections.
#include "memmap.h"
static inline procmap_t procmap_default_mk(unsigned dom) {
    enum { MB = 1024 * 1024 };

    procmap_t p = {};

    // Pi 3 BCM2837 peripherals (3 contiguous MB at 0x3F000000)
    procmap_push(&p, pr_ent_mk(0x3F000000, MB, MEM_DEVICE, dom));
    procmap_push(&p, pr_ent_mk(0x3F100000, MB, MEM_DEVICE, dom));
    procmap_push(&p, pr_ent_mk(0x3F200000, MB, MEM_DEVICE, dom));

    // Pi 3 ARM local peripherals at 0x40000000
    procmap_push(&p, pr_ent_mk(0x40000000, MB, MEM_DEVICE, dom));

    // code: 0x0 - 0x100000
    assert((uint32_t)__prog_end__ <= MB);
    procmap_push(&p, pr_ent_mk(0x00000000, MB, MEM_RW, dom));

    // heap: 1MB at 0x100000
    char *start = kmalloc_heap_start();
    char *end = kmalloc_heap_end();
    unsigned nbytes = end - start;

    if(start != (void*)MB)
        panic("heap starts at %p, expected 1MB\n", start);
    if(nbytes != MB)
        panic("heap is %d bytes, expected 1MB\n", nbytes);

    procmap_push(&p, pr_ent_mk(0x00100000, MB, MEM_RW, dom));

    // the two hardcoded stacks we use.
    procmap_push(&p, pr_ent_mk(INT_STACK_ADDR-MB, MB, MEM_RW, dom));
    procmap_push(&p, pr_ent_mk(STACK_ADDR-MB, MB, MEM_RW, dom));

    return p;
}

#endif

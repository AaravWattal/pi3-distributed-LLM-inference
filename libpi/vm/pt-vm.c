#include "rpi.h"
#include "libc/demand.h"
#include "pt-vm.h"
#include "libc/helper-macros.h"
#include "procmap.h"

// turn this off if you don't want all the debug output.
enum { verbose_p = 0 };
enum { OneMB = 1024*1024 };

vm_pt_t *vm_pt_alloc(unsigned n) {
    demand(n == 4096, we only handling a fully-populated page table right now);

    vm_pt_t *pt = 0;
    unsigned nbytes = n * sizeof *pt;

    // trivial:
    // allocate pt with n entries [should look just like you did 
    // for pinned vm]
    // pt = staff_vm_pt_alloc(n);
    pt = kmalloc_aligned(4096*4, 1<<14);

    demand(is_aligned_ptr(pt, 1<<14), must be 14-bit aligned!);
    return pt;
}

// allocate new page table and copy pt.  not the
// best interface since it will copy private mappings.
vm_pt_t *vm_dup(vm_pt_t *pt1) {
    vm_pt_t *pt2 = vm_pt_alloc(PT_LEVEL1_N);
    memcpy(pt2,pt1,PT_LEVEL1_N * sizeof *pt1);
    return pt2;
}

// same as pinned version: 
//  - probably should check that the page table 
//    is set, and asid makes sense.
void vm_mmu_enable(void) {
    assert(!mmu_is_enabled());
    mmu_enable();
    assert(mmu_is_enabled());
}

// same as pinned 
void vm_mmu_disable(void) {
    assert(mmu_is_enabled());
    mmu_disable();
    assert(!mmu_is_enabled());
}

// - set <pt,pid,asid> for an address space.
// - must be done before you switch into it!
// - mmu can be off or on.
void vm_mmu_switch(vm_pt_t *pt, uint32_t pid, uint32_t asid) {
    assert(pt);
    mmu_set_ctx(pid, asid, pt);
}

// just like pinned.
void vm_mmu_init(uint32_t domain_reg) {
    // initialize everything, after bootup.
    mmu_init();
    domain_access_ctrl_set(domain_reg);
}

// armv7 section descriptor: tag=1, PXN=0, NS=0.
vm_pte_t *
vm_map_sec(vm_pt_t *pt, uint32_t va, uint32_t pa, pin_t attr) 
{
    assert(aligned(va, OneMB));
    assert(aligned(pa, OneMB));

    // today we just use 1mb.
    assert(attr.pagesize == PAGE_1MB);

    unsigned index = va >> 20;
    assert(index < PT_LEVEL1_N);

    // 1. lookup the <pte> in <pt> using index.
    vm_pte_t *pte = pt + index;

    // 2. use values in <attr> to set pte's:
    pte->PXN = 0;
    pte->tag = 1;
    pte->B = attr.mem_attr & 1;
    pte->C = (attr.mem_attr & 0b10) >> 1;
    pte->XN = 0;
    pte->domain = attr.dom;
    pte->IMP = 0;
    pte->AP = attr.AP_perm & 0b11;
    pte->TEX = attr.mem_attr >> 2;
    pte->AP2 = attr.AP_perm >> 2;
    pte->S = 0;
    pte->nG = !attr.G;
    pte->super = 0;
    pte->NS = 0;

    // 3. set <pte->sec_base_addr> to the right physical section.
    pte->sec_base_addr = pa >> 20;

    // 4. you modified the page table!  
    //   - make sure you call your sync PTE (lab 15).
    mmu_sync_pte_mods();

    if(verbose_p)
        vm_pte_print(pt,pte);
    assert(pte);
    return pte;
}

// lookup 32-bit address va in pt and return the pte
// if it exists, return 0 otherwise (use tag)
vm_pte_t * vm_lookup(vm_pt_t *pt, uint32_t va) {
    uint32_t pte_idx = va >> 20;

    vm_pt_t* pte = pt + pte_idx;

    // armv7: tag is bit[1], must be 1 for section
    if (pte->tag != 1) {
        return 0;
    }

    return pte;
}

// manually translate <va> in page table <pt>
// - if doesn't exist, return 0.
// - if does exist:
//    1. write the translated physical address to <*pa> 
//    2. return the pte pointer.
//
// NOTE: 
//   - we can't just return the <pa> b/c page 0 could be mapped.
//   - the common unix kernel hack of returning (void*)-1 leads
//     to really really nasty bugs.  so we don't.
vm_pte_t *vm_xlate(uint32_t *pa, vm_pt_t *pt, uint32_t va) {
    vm_pte_t* pte = vm_lookup(pt, va);

    if (!pte) {
        return 0;
    }

    *pa = (pte->sec_base_addr << 20) | (va & 0xFFFFF);
    return pte;
}

// compute the default attribute for each type.
static inline pin_t attr_mk(pr_ent_t *e) {
    switch(e->type) {
    case MEM_DEVICE: 
        return pin_mk_device(e->dom);
    // kernel: currently everything is uncached.
    case MEM_RW:
        return pin_mk_global(e->dom, perm_rw_priv, MEM_uncached);
   case MEM_RO: 
        panic("not handling\n");
   default: 
        panic("unknown type: %d\n", e->type);
    }
}

// setup the initial kernel mapping.  This will mirror
//    static inline void procmap_pin_on(procmap_t *p) 
// in <13-pinned-vm/code/procmap.h>  but will call
// your vm_ routines, not pinned routines.
//
// if <enable_p>=1, should enable the MMU.  make sure
// you setup the page table and asid. use  
// kern_asid, and kern_pid.
vm_pt_t *vm_map_kernel(procmap_t *p, int enable_p) {
    // the asid and pid we start with.  
    //    shouldn't matter since kernel is global.
    enum { kern_asid = 1, kern_pid = 0x140e };

    // 1. compute domains being used.
    // 2. call <vm_mmu_init> to set domain reg (using 1) and init MMU.
    // 3. allocate a page table <vm_pt_alloc>
    // 4. walk through procmap, mapping each entry <vm_map_sec>
    //    - note: for today we use identity map, so <addr> --> <addr>
    // 5. use <vm_mmu_switch> to setup <kern_asid>, <pt>, and <kern_pid>
    // 6. if <enable_p>=1
    //    - <mmu_sync_pte_mods> since modified page table.
    //    - <vm_mmu_enable> to turn on

    uint32_t d = dom_perm(p->dom_ids, DOM_client);

    vm_mmu_init(d);

    vm_pt_t *pt = vm_pt_alloc(4096);

    for (int i = 0; i < p->n; i++) {
        pr_ent_t cur_pr_ent = p->map[i];
        pin_t attr = attr_mk(&cur_pr_ent);

        vm_pte_t* allocated_pte = vm_map_sec(pt, cur_pr_ent.addr, cur_pr_ent.addr, attr);

        vm_pte_t* lookup_pte = vm_lookup(pt, cur_pr_ent.addr);

        assert(lookup_pte);
        assert(allocated_pte == lookup_pte);
    }

    vm_mmu_switch(pt, kern_pid, kern_asid);

    if (enable_p == 1) {
        mmu_sync_pte_mods();
        vm_mmu_enable();
    }

    // return page table.
    assert(pt);
    return pt;
}

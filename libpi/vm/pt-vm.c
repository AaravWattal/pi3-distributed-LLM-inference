#include "rpi.h"
#include "libc/demand.h"
#include "pt-vm.h"
#include "libc/helper-macros.h"
#include "procmap.h"

#define aligned(x, a) (((x) & ((a)-1)) == 0)
#define is_aligned_ptr(p, a) (((unsigned)(p) & ((a)-1)) == 0)

enum { verbose_p = 0 };
enum { OneMB = 1024*1024 };

vm_pt_t *vm_pt_alloc(unsigned n) {
    demand(n == 4096, we only handling a fully-populated page table right now);

    vm_pt_t *pt = 0;

    pt = kmalloc_aligned(4096*4, 1<<14);

    demand(is_aligned_ptr(pt, 1<<14), must be 14-bit aligned!);
    return pt;
}

vm_pt_t *vm_dup(vm_pt_t *pt1) {
    vm_pt_t *pt2 = vm_pt_alloc(PT_LEVEL1_N);
    memcpy(pt2,pt1,PT_LEVEL1_N * sizeof *pt1);
    return pt2;
}

void vm_mmu_enable(void) {
    assert(!mmu_is_enabled());
    mmu_enable();
    assert(mmu_is_enabled());
}

void vm_mmu_disable(void) {
    assert(mmu_is_enabled());
    mmu_disable();
    assert(!mmu_is_enabled());
}

void vm_mmu_switch(vm_pt_t *pt, uint32_t pid, uint32_t asid) {
    assert(pt);
    mmu_set_ctx(pid, asid, pt);
}

void vm_mmu_init(uint32_t domain_reg) {
    mmu_init();
    domain_access_ctrl_set(domain_reg);
}

// ARMv7 section descriptor: tag=1, PXN=0, NS=0.
vm_pte_t *
vm_map_sec(vm_pt_t *pt, uint32_t va, uint32_t pa, pin_t attr) 
{
    assert(aligned(va, OneMB));
    assert(aligned(pa, OneMB));
    assert(attr.pagesize == PAGE_1MB);

    unsigned index = va >> 20;
    assert(index < PT_LEVEL1_N);

    vm_pte_t *pte = pt + index;

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

    pte->sec_base_addr = pa >> 20;

    mmu_sync_pte_mods();

    if(verbose_p)
        vm_pte_print(pt,pte);
    assert(pte);
    return pte;
}

vm_pte_t * vm_lookup(vm_pt_t *pt, uint32_t va) {
    uint32_t pte_idx = va >> 20;

    vm_pt_t* pte = pt + pte_idx;

    // ARMv7: tag is bit[1], must be 1 for section
    if (pte->tag != 1) {
        return 0;
    }

    return pte;
}

vm_pte_t *vm_xlate(uint32_t *pa, vm_pt_t *pt, uint32_t va) {
    vm_pte_t* pte = vm_lookup(pt, va);

    if (!pte) {
        return 0;
    }

    *pa = (pte->sec_base_addr << 20) | (va & 0xFFFFF);
    return pte;
}

static inline pin_t attr_mk(pr_ent_t *e) {
    switch(e->type) {
    case MEM_DEVICE: 
        return pin_mk_device(e->dom);
    case MEM_RW:
        return pin_mk_global(e->dom, perm_rw_priv, MEM_uncached);
   case MEM_RO: 
        panic("not handling\n");
   default: 
        panic("unknown type: %d\n", e->type);
    }
}

vm_pt_t *vm_map_kernel(procmap_t *p, int enable_p) {
    enum { kern_asid = 1, kern_pid = 0x140e };

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

    assert(pt);
    return pt;
}

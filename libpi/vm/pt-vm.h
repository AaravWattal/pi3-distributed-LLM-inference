#ifndef __VM_H__
#define __VM_H__

#include "mem-attr.h"
#include "armv6-vm.h"
#include "mmu.h"

#include "procmap.h"

// bad design: we need <pin_t>
#include "pinned-vm.h"

typedef fld_t vm_pt_t;

// page table entry (confusingly same type since its an array)
typedef fld_t vm_pte_t;

// ARMv7: tag=1, PXN=0 for section descriptor
static inline vm_pte_t vm_pte_mk(void) {
    return (vm_pte_t){ .tag = 1, .PXN = 0 };
}


// 4gb / 1mb = 4096 entries fully populated for first level
// of page table.
enum { PT_LEVEL1_N = 4096 };


// allocate zero-filled page table with correct alignment.
vm_pt_t *vm_pt_alloc(unsigned nentries);
vm_pt_t *staff_vm_pt_alloc(unsigned nentries);

vm_pte_t *vm_map_sec(vm_pt_t *pt, uint32_t va, uint32_t pa, pin_t attr);
vm_pte_t *staff_vm_map_sec(vm_pt_t *pt, uint32_t va, uint32_t pa, pin_t attr);

void vm_mmu_enable(void);
void vm_mmu_disable(void);

// mmu can be off or on.
void vm_mmu_switch(vm_pt_t *pt, uint32_t pid, uint32_t asid);

enum { dom_all_access = ~0, dom_no_access = 0 };

void vm_mmu_init(uint32_t domain_reg);


// lookup va in page table.
vm_pte_t * vm_lookup(vm_pt_t *pt, uint32_t va);
vm_pte_t * staff_vm_lookup(vm_pt_t *pt, uint32_t va);

vm_pte_t *vm_xlate(uint32_t *pa, vm_pt_t *pt, uint32_t va);
vm_pte_t *staff_vm_xlate(uint32_t *pa, vm_pt_t *pt, uint32_t va);

vm_pt_t *vm_dup(vm_pt_t *pt);

vm_pt_t *vm_map_kernel(procmap_t *p, int enable_p);
vm_pt_t *staff_vm_map_kernel(procmap_t *p, int enable_p);

// arm-vm-helpers: print <f>
void vm_pte_print(vm_pt_t *pt, vm_pte_t *pte);

void vm_mprotect(vm_pt_t *pt, unsigned va, unsigned nsec, pin_t perm);


#define mem_attr_TEX(m) bits_get(m,2,4)
#define mem_attr_B(m) bit_get(m,0)
#define mem_attr_C(m) bit_get(m,1)



#endif

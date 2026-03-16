#ifndef __ARM_VM_H__
#define __ARM_VM_H__

/**************************************************************************
 * Structures for ARMv7 virtual memory (short-descriptor format).
 * Ported from ARMv6 (ARM1176JZF-S) to ARMv7 (Cortex-A53 AArch32).
 *
 * Page table operations: we only handle mapping 1MB sections.
 *
 * ARMv7 section descriptor layout (B3.5.1, Figure B3-4):
 *
 *   [31:20] Section base address PA[31:20]
 *   [19]    NS (Non-secure bit, write 0 for bare-metal secure)
 *   [18]    0 = Section, 1 = Supersection
 *   [17]    nG (not-Global)
 *   [16]    S (Shareable)
 *   [15]    AP[2]  (was called APX in ARMv6)
 *   [14:12] TEX[2:0]
 *   [11:10] AP[1:0]
 *   [9]     IMPLEMENTATION DEFINED
 *   [8:5]   Domain[3:0]
 *   [4]     XN (Execute-Never)
 *   [3]     C
 *   [2]     B
 *   [1]     1  (descriptor type bit, must be 1 for section)
 *   [0]     PXN (Privileged Execute-Never, new in ARMv7)
 */

typedef struct first_level_descriptor {
    unsigned
        PXN:1,              // 0:1      PXN — new in ARMv7, write 0 if not wanted
        tag:1,              // 1:1      must be 1 for section descriptor
        B:1,                // 2:1
        C:1,                // 3:1
        XN:1,               // 4:1      1 = execute never, 0 = can execute

        domain:4,           // 5-8:4    domain id
        IMP:1,              // 9:1      should be 0

        AP:2,               // 10-11:2  permissions AP[1:0]
        TEX:3,              // 12-14:3
        AP2:1,              // 15:1     AP[2] (was APX in ARMv6)
        S:1,                // 16:1     shareable
        nG:1,               // 17:1     nG=0 ==> global, =1 ==> process specific
        super:1,            // 18:1     0=section, 1=supersection
        NS:1,               // 19:1     non-secure bit (write 0 for bare-metal)
        sec_base_addr:12;   // 20-31    must be aligned
} fld_t;
_Static_assert(sizeof(fld_t) == 4, "invalid size for fld_t!");

// B4-9: AP field:  no/access=0b00, r/o=0b10, rw=0b11
enum {
    AP_rw = 0b11,
    AP_no_access = 0b00,
    AP_rd_only = 0b10
};

// arm-vm-helpers: print <f>
// void fld_print(fld_t *f);


/***********************************************************************
 * utility routines [in mmu-helpers.c]
 */

// hash [data, data+n) and print it with <msg>
void hash_print(const char *msg, const void *data, unsigned n);

// one-time sanity check of the offsets in the structures above.
void check_vm_structs(void);

// domain access control
void domain_acl_print(void);

#endif

/*
 * use interrupts to implement a simple statistical profiler.
 *	- interrupt code is a replication of ../timer-int/timer.c
 *	- you'll need to implement kmalloc so you can allocate 
 *	  a histogram table from the heap.
 *	- implement functions so that given a pc value, you can increment
 *	  its associated count
 */
#include "rpi.h"

// pulled the interrupt code into these header files.
#include "rpi-interrupts.h"
#include "timer-int.h"

// defines C externs for the labels defined by the linker script
// libpi/memmap.
// 
// you can use these to get the size of the code segment, 
// data segment, etc.
#include "memmap.h"

/*************************************************************
 * gprof implementation:
 *	- allocate a table with one entry for each instruction.
 *	- gprof_init(void) - call before starting.
 *	- gprof_inc(pc) will increment pc's associated entry.
 *	- gprof_dump(n) prints the top n samples by count.
 */

static unsigned hist_n, pc_min, pc_max;
static volatile unsigned *hist = 0;

// - compute <pc_min>, <pc_max> using the 
//   <libpi/memmap> symbols: 
//   - use for bounds checking.
// - allocate <hist> with <kmalloc> using <pc_min> and
//   <pc_max> to compute code size.
static unsigned gprof_init(void) {
    // todo("allocate <hist> using <kmalloc>.  initialize etc\n");
    pc_min = (unsigned)__code_start__;
    pc_max = (unsigned)__code_end__;

    hist_n = pc_max - pc_min;
    hist = kmalloc(hist_n);
    memset((void *)hist, 0, hist_n);

    return hist_n;
}

// increment histogram associated w/ pc.
//    few lines of code
static void gprof_inc(unsigned pc) {
    assert(pc >= pc_min && pc <= pc_max);
    // todo("make sure you bounds check\n");
    // unimplemented();
    volatile unsigned *hist_pointer = hist + (pc - pc_min) / 4;
    *hist_pointer += 1;
}

#define GPROF_MAX_CANDIDATES 256
// print out top <n_candidates> samples by count (descending).
// n_candidates is capped at GPROF_MAX_CANDIDATES.
//
// make sure gprof does not sample this code!
// we don't care where it spends time.
//
// how to validate:
//  - take the addresses and look in <gprof.list>
//  - we expect pc's to be in GET32, PUT32, different
//    uart routines, or rpi_wait.  (why?)
static void gprof_dump(unsigned n_candidates) {
    printk("gprof_dump: n_candidates=%u\n", n_candidates);
    if (n_candidates == 0)
        return;
    if (n_candidates > GPROF_MAX_CANDIDATES)
        n_candidates = GPROF_MAX_CANDIDATES;

    unsigned top_pc[GPROF_MAX_CANDIDATES];
    unsigned top_cnt[GPROF_MAX_CANDIDATES];
    unsigned n_top = 0;

    for (unsigned pc = pc_min; pc < pc_max; pc += 4) {
        unsigned cnt = hist[(pc - pc_min) / 4];
        if (cnt == 0)
            continue;

        if (n_top < n_candidates) {
            top_pc[n_top] = pc;
            top_cnt[n_top] = cnt;
            n_top++;
        } else {
            unsigned min_idx = 0;
            for (unsigned i = 1; i < n_top; i++)
                if (top_cnt[i] < top_cnt[min_idx])
                    min_idx = i;
            if (cnt > top_cnt[min_idx]) {
                top_pc[min_idx] = pc;
                top_cnt[min_idx] = cnt;
            }
        }
    }

    for (unsigned i = 0; i < n_top; i++) {
        unsigned max_idx = i;
        for (unsigned j = i + 1; j < n_top; j++)
            if (top_cnt[j] > top_cnt[max_idx])
                max_idx = j;
        if (max_idx != i) {
            unsigned tpc = top_pc[i], tcnt = top_cnt[i];
            top_pc[i] = top_pc[max_idx]; top_cnt[i] = top_cnt[max_idx];
            top_pc[max_idx] = tpc; top_cnt[max_idx] = tcnt;
        }
        printk("PC=%x\tcount=%u\n", top_pc[i], top_cnt[i]);
    }
}

/**************************************************************
 * timer interrupt code from before, now calls gprof update.
 */
// Q: if you make not volatile?
static volatile unsigned cnt;
static volatile unsigned period;

// Stub handlers for unused ARM exception vectors (should not occur).
void reset_vector(unsigned pc) { panic("unexpected reset, pc=%x\n", pc); }
void undefined_instruction_vector(unsigned pc) { panic("undefined instruction, pc=%x\n", pc); }
void syscall_vector(unsigned pc) { panic("unexpected syscall, pc=%x\n", pc); }
void prefetch_abort_vector(unsigned pc) { panic("prefetch abort, pc=%x\n", pc); }
void data_abort_vector(unsigned pc) { panic("data abort, pc=%x\n", pc); }
void fast_interrupt_vector(unsigned pc) { panic("unexpected FIQ, pc=%x\n", pc); }

// client has to define this.
void interrupt_vector(unsigned pc) {
    dev_barrier();
    unsigned pending = GET32(IRQ_basic_pending);
    if((pending & ARM_Timer_IRQ) == 0)
        return;

    PUT32(ARM_Timer_IRQ_Clear, 1);
    cnt++;

    // increment the counter for <pc>.
    gprof_inc(pc);

    // this doesn't need to stay here.
    static unsigned last_clk = 0;
    unsigned clk = timer_get_usec();
    period = last_clk ? clk - last_clk : 0;
    last_clk = clk;

    dev_barrier();
}

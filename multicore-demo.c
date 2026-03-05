#include "multicore.h"
#include "rpi.h"
#include "uart.h"

#define WORK_ITERATIONS 5000000

typedef struct {
    uint32_t* output;
} compute_args_t;

static void compute_sum_squares(void* arg) {
    compute_args_t* a = (compute_args_t*)arg;

    uint32_t sum = 0;

    for (uint32_t i = 1; i <= WORK_ITERATIONS; i++) {
        sum += i * i;
    }

    *a->output = sum;
}

/* Run the same three tasks on core 0, one after another. */
static void run_sequential(uint32_t* r1, uint32_t* r2, uint32_t* r3) {
    compute_args_t args1 = { .output = r1 };
    compute_args_t args2 = { .output = r2 };
    compute_args_t args3 = { .output = r3 };

    compute_sum_squares(&args1);
    compute_sum_squares(&args2);
    compute_sum_squares(&args3);
}

static void run_parallel(uint32_t* r1, uint32_t* r2, uint32_t* r3) {
    compute_args_t args1 = { .output = r1 };
    compute_args_t args2 = { .output = r2 };
    compute_args_t args3 = { .output = r3 };

    int status = multicore_call_async(1, compute_sum_squares, &args1);
    
    if (status != MULTICORE_OK) {
        panic("Core 1 call failed\n");
    }

    status = multicore_call_async(2, compute_sum_squares, &args2);

    if (status != MULTICORE_OK) {
        panic("Core 2 call failed\n");
    }

    multicore_call_async(3, compute_sum_squares, &args3);
    
    if (status != MULTICORE_OK) {
        panic("Core 3 call failed\n");
    }

    status = multicore_wait_all();

    if (status != MULTICORE_OK) {
        panic("Core wait all failed\n");
    }
}

void run_multicore_demo(void) {
    printk("Multicore demo: sum of squares 1..%d\r\n", WORK_ITERATIONS);

    multicore_init();

    uint32_t seq1 = 0, seq2 = 0, seq3 = 0;
    uint32_t par1 = 0, par2 = 0, par3 = 0;

    uint32_t t0 = timer_get_usec();
    run_sequential(&seq1, &seq2, &seq3);
    uint32_t t1 = timer_get_usec();

    uint32_t t2 = timer_get_usec();
    run_parallel(&par1, &par2, &par3);
    uint32_t t3 = timer_get_usec();

    uint32_t usec_seq = t1 - t0;
    uint32_t usec_par = t3 - t2;

    printk("Sequential: %u usec\r\n", usec_seq);
    printk("Parallel: %u usec\r\n", usec_par);

    if (seq1 == par1 && seq2 == par2 && seq3 == par3) {
        printk("Results match.\r\n");
    } else {
        printk("MISMATCH: sequential vs parallel results differ!\r\n");
    }

    if (usec_seq > 0 && usec_par > 0) {
        printk("Approx speedup: %u x\r\n", usec_seq / usec_par);
    }
}

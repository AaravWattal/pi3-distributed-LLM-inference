#include "multicore.h"
#include "rpi.h"
#include "uart.h"

typedef struct {
    int input;
    int* output;
} square_args_t;

static void compute_square(void* arg) {
    square_args_t* a = (square_args_t*)arg;
    *a->output = a->input * a->input;
}

void run_multicore_demo(void) {
    printk("Starting multicore demo\r\n");

    multicore_init();

    int result1 = 0;
    int result2 = 0;
    int result3 = 0;
    square_args_t args1 = { .input = 3, .output = &result1 };
    square_args_t args2 = { .input = 5, .output = &result2 };
    square_args_t args3 = { .input = 7, .output = &result3 };

    int status;

    status = multicore_call_async(1, compute_square, &args1);
    if (status != MULTICORE_OK) {
        printk("Core 1 call failed: %d\r\n", status);
        return;
    }
    status = multicore_call_async(2, compute_square, &args2);
    if (status != MULTICORE_OK) {
        printk("Core 2 call failed: %d\r\n", status);
        return;
    }
    status = multicore_call_async(3, compute_square, &args3);
    if (status != MULTICORE_OK) {
        printk("Core 3 call failed: %d\r\n", status);
        return;
    }

    multicore_wait(1);
    multicore_wait(2);
    multicore_wait(3);

    printk("Core 1: 3^2 = %d\r\n", result1);
    printk("Core 2: 5^2 = %d\r\n", result2);
    printk("Core 3: 7^2 = %d\r\n", result3);
    printk("Multicore demo done\r\n");
}

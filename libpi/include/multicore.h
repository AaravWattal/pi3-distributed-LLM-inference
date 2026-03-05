/*********************************************************
 * multicore stuff
 */

#define NUM_CORES 4

void multicore_init(void);
void multicore_main(unsigned core_id);
void command_loop(unsigned core_id);

unsigned multicore_get_core_id(void);

// Runs a specified function on a specific core
int multicore_call(unsigned core_id, int (*func)(void*), void* arg, int* result);
int multicore_call_async(unsigned core_id, int (*func)(void*), void* arg);

// Checks if specified core has finished running its function
int multicore_check_done(unsigned core_id);

// Waits for a specified core to finish running its function
// Is blocking
int multicore_wait(unsigned core_id);

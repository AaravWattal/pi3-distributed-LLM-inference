/*********************************************************
 * multicore stuff
 */

#define NUM_CORES 4

// Status codes
#define MULTICORE_OK        0
#define MULTICORE_ERR_CORE  -1      // Invalid core ID
#define MULTICORE_ERR_NULL  -2      // Null function pointer
#define MULTICORE_ERR_BUSY  -3      // Core still running previous task

// Worker function type
typedef void (*multicore_worker_t)(void* arg);

// Defined in multicore-start.S
extern void multicore_entry(void);

void multicore_init(void);
void multicore_main(unsigned core_id);
void command_loop(unsigned core_id);

unsigned multicore_get_core_id(void);

// Runs worker function on a core
int multicore_call(unsigned core_id, multicore_worker_t func, void* arg);

// Asynchronous multicore_call
int multicore_call_async(unsigned core_id, multicore_worker_t func, void* arg);

// Checks if specified core has finished running its function
int multicore_check_done(unsigned core_id);

// Waits for a specified core to finish running its function
// Is blocking
int multicore_wait(unsigned core_id);

// Waits for all cores to finish running their functions
int multicore_wait_all(void);

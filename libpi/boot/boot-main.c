/*                  DO NOT MODIFY THIS CODE.
 *                  DO NOT MODIFY THIS CODE.
 *                  DO NOT MODIFY THIS CODE.
 *                  DO NOT MODIFY THIS CODE.
 *
 * engler, cs140e: trivial driver for your get_code implementation,
 * which does the actual getting and loading of the code.
 */
#include "get-code.h"
#include "rpi.h"
#include "libc/demand.h"

void notmain(void) {
    uint32_t addr = get_code();
    if(!addr)
        rpi_reboot();

    // blx to addr.  
    // could also call it as a function pointer.
    BRANCHTO(addr);
    not_reached();
}

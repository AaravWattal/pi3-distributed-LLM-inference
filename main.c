#include "rpi.h"
#include "libc/demand.h"
#include "uart.h"

void notmain_llama_inference(void);

void notmain(void) {
    notmain_llama_inference();
}

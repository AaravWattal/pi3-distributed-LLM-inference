#include "uart.h"

int main(void) {
    uart_init();

    for (int i = 0; ; i++) {
        uart_put8('H');
        uart_put8('e');
        uart_put8('l');
        uart_put8('l');
        uart_put8('o');
        uart_put8(',');
        uart_put8(' ');
        uart_put8('w');
        uart_put8('o');
        uart_put8('r');
        uart_put8('l');
        uart_put8('d');
        uart_put8('\r');
        uart_put8('\n');
        for (volatile unsigned long d = 0; d < 500000; d++) { }
    }

    return 0;
}
#include "uart.h"

int main(void) {
    uart_init();

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
    uart_put8('\n');

    return 0;
}
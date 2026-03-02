#include <stdint.h>

// initialize [XXX: we should take a baud rate?]
void uart_init(void);
// disable
void uart_disable(void);

// get one byte from the uart
int uart_get8(void);
// put one byte on the uart:
// returns < 0 on error.
int uart_put8(uint8_t c);

int uart_hex(unsigned h);

// returns -1 if no byte, the value otherwise.
int uart_get8_async(void);

// 0 = no data, 1 = at least one byte
int uart_has_data(void);

// 0 = no space, 1 = space for at least 1 byte
int uart_can_put8(void);
int uart_can_putc(void);

// flush out the tx fifo
void uart_flush_tx(void);
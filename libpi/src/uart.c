#include "rpi.h"
#include "gpio.h"
#include "uart.h"

// pl011 uart registers
// rpi3 base 0x3F201000
// bcm2835 chapter 13
enum {
    PL011_DR   = 0x3F201000,   // data register
    PL011_FR   = 0x3F201018,   // flag register
    PL011_IBRD = 0x3F201024,   // integer baud-rate divisor
    PL011_FBRD = 0x3F201028,   // fractional baud-rate divisor
    PL011_LCRH = 0x3F20102C,   // line control register
    PL011_CR   = 0x3F201030,   // control register
    PL011_IMSC = 0x3F201038,   // interrupt mask set/clear
    PL011_ICR  = 0x3F201044,   // interrupt clear register
};

enum {
    FR_BUSY = (1 << 3),   // uart busy transmitting
    FR_RXFE = (1 << 4),   // rx FIFO empty
    FR_TXFF = (1 << 5),   // tx FIFO full
    FR_TXFE = (1 << 7),   // tx FIFO empty + idle
};

enum {
    CR_UARTEN = (1 << 0),   // uart enable
    CR_TXE    = (1 << 8),   // tx enable
    CR_RXE    = (1 << 9),   // rx enable
};

enum {
    LCRH_FEN    = (1 << 4),     // enable fifos
    LCRH_WLEN_8 = (0x3 << 5),   // 8 bit words
};

// called first to setup uart to 8n1 115200  baud,
// no interrupts.
//  - you will need memory barriers, use <dev_barrier()>
//
//  later: should add an init that takes a baud rate.
void uart_init(void) {
    dev_barrier();

    // Set up GPIO pins
    gpio_set_function(14, GPIO_FUNC_ALT0);
    gpio_set_function(15, GPIO_FUNC_ALT0);
    gpio_pud_off(14);
    gpio_pud_off(15);

    dev_barrier();

    // Disable UART before reconfiguring
    PUT32(PL011_CR, 0);

    // Finish pror work
    while (GET32(PL011_FR) & FR_BUSY)
        rpi_wait();

    // Clear interrupts
    PUT32(PL011_ICR, 0x7FF);

    // Set baudrate
    PUT32(PL011_IBRD, 26);
    PUT32(PL011_FBRD, 3);

    // Set 8n1 mode
    // Enable fifos
    PUT32(PL011_LCRH, LCRH_WLEN_8 | LCRH_FEN);

    // Mask all interrupts
    PUT32(PL011_IMSC, 0);

    // Enable UART, TX, RX
    PUT32(PL011_CR, CR_UARTEN | CR_TXE | CR_RXE);

    dev_barrier();
}

// disable the uart: make sure all bytes have been
// 
void uart_disable(void) {
    dev_barrier();

    uart_flush_tx();

    PUT32(PL011_CR, 0);

    dev_barrier();
}

// returns one byte from the RX (input) hardware
// FIFO.  if FIFO is empty, blocks until there is 
// at least one byte.
int uart_get8(void) {
    dev_barrier();

    while (GET32(PL011_FR) & FR_RXFE) {
        rpi_wait();
    }

    int c = GET32(PL011_DR) & 0xFF;

    dev_barrier();

    return c;
}

// returns 1 if the hardware TX (output) FIFO has room
// for at least one byte.  returns 0 otherwise.
int uart_can_put8(void) {
    dev_barrier();

    int can_put8 = !(GET32(PL011_FR) & FR_TXFF);

    dev_barrier();

    return can_put8;
}

// put one byte on the TX FIFO, if necessary, waits
// until the FIFO has space.
int uart_put8(uint8_t c) {
    dev_barrier();

    while (GET32(PL011_FR) & FR_TXFF) {
        rpi_wait();
    }

    PUT32(PL011_DR, c);

    dev_barrier();

    return 1;
}

// returns:
//  - 1 if at least one byte on the hardware RX FIFO.
//  - 0 otherwise
int uart_has_data(void) {
    dev_barrier();

    int has_data = !(GET32(PL011_FR) & FR_RXFE);

    dev_barrier();

    return has_data;
}

// returns:
//  -1 if no data on the RX FIFO.
//  otherwise reads a byte and returns it.
int uart_get8_async(void) { 
    if(!uart_has_data())
        return -1;
    return uart_get8();
}

// returns:
//  - 1 if TX FIFO empty AND idle.
//  - 0 if not empty.
int uart_tx_is_empty(void) {
    dev_barrier();

    unsigned fr = GET32(PL011_FR);
    int done = (fr & FR_TXFE) && !(fr & FR_BUSY);

    dev_barrier();

    return done;
}

// return only when the TX FIFO is empty AND the
// TX transmitter is idle.  
//
// used when rebooting or turning off the UART to
// make sure that any output has been completely 
// transmitted.  otherwise can get truncated 
// if reboot happens before all bytes have been
// received.
void uart_flush_tx(void) {
    while(!uart_tx_is_empty())
        rpi_wait();
}

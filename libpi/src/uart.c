#include "gpio.h"
#include "uart.h"

enum {
    AUX_ENB = 0x3F215004,
    AUX_MU_IO = 0x3F215040,
    AUX_MU_IER = 0x3F215044,
    AUX_MU_IIR = 0x3F215048,
    AUX_MU_LCR = 0x3F21504C,
    AUX_MU_MCR = 0x3F215050,
    AUX_MU_LSR = 0x3F215054,
    AUX_MU_STAT = 0x3F215064,
    AUX_MU_BAUD = 0x3F215068,
    AUX_MU_CTRL = 0x3F215060
};

// called first to setup uart to 8n1 115200  baud,
// no interrupts.
//  - you will need memory barriers, use <dev_barrier()>
//
//  later: should add an init that takes a baud rate.
void uart_init(void) {
    dev_barrier();

    // Set up GPIO pins
    gpio_set_function(14, GPIO_FUNC_ALT5);
    gpio_set_function(15, GPIO_FUNC_ALT5);

    dev_barrier();

    // Enable UART
    unsigned aux_enb_val = GET32(AUX_ENB);
    unsigned lower_bit_mask = 0x1;
    PUT32(AUX_ENB, aux_enb_val | lower_bit_mask);

    dev_barrier();

    // Disable TX/RX
    PUT32(AUX_MU_CTRL, 0x0);

    // Clear FIFOs
    unsigned bits_2_1_on = 0x6;
    PUT32(AUX_MU_IIR, bits_2_1_on);

    // Disable interrupts
    PUT32(AUX_MU_IER, 0x0);

    // Set baudrate
    PUT32(AUX_MU_BAUD, 270);

    // Set 8n1 mode
    PUT32(AUX_MU_LCR, 0x3);

    // Clear MCR
    PUT32(AUX_MU_MCR, 0x0);

    // Now enable TX/RX
    PUT32(AUX_MU_CTRL, 0x3);

    dev_barrier();

    // NOTE: for cross-checking: make sure write UART 
    // addresses in order

}

// disable the uart: make sure all bytes have been
// 
void uart_disable(void) {
    dev_barrier();

    uart_flush_tx();

    unsigned aux_enb_val = GET32(AUX_ENB);
    unsigned lower_bit_mask = 0x1;

    PUT32(AUX_ENB, aux_enb_val & ~lower_bit_mask);

    dev_barrier();
}

// returns one byte from the RX (input) hardware
// FIFO.  if FIFO is empty, blocks until there is 
// at least one byte.
int uart_get8(void) {
    dev_barrier();

    // Wait for new data
    while (1) {
        unsigned aux_stat_val = GET32(AUX_MU_STAT);
        unsigned receive_level = (aux_stat_val & (0x3 << 16)) >> 16;

        if (receive_level > 0) {
            break;
        }

        rpi_wait();
    }

    // Read in data
    unsigned aux_io_val = GET32(AUX_MU_IO);
    unsigned lower_8_bit_mask = 0xFF;

    dev_barrier();

    return aux_io_val & lower_8_bit_mask;
}

// returns 1 if the hardware TX (output) FIFO has room
// for at least one byte.  returns 0 otherwise.
int uart_can_put8(void) {
    dev_barrier();

    unsigned aux_lsr_val = GET32(AUX_MU_STAT);
    unsigned can_put8 = (aux_lsr_val & 0x2) >> 1;

    dev_barrier();

    return can_put8;
}

// put one byte on the TX FIFO, if necessary, waits
// until the FIFO has space.
int uart_put8(uint8_t c) {
    dev_barrier();

    while (1) {
        unsigned aux_stat_val = GET32(AUX_MU_STAT);
        unsigned can_put8 = (aux_stat_val & 0x2) >> 1;

        if (can_put8) {
            break;
        }

        rpi_wait();
    }

    PUT32(AUX_MU_IO, c);

    dev_barrier();

    return 1;
}

// returns:
//  - 1 if at least one byte on the hardware RX FIFO.
//  - 0 otherwise
int uart_has_data(void) {
    dev_barrier();

    unsigned aux_stat_val = GET32(AUX_MU_STAT);
    unsigned has_data = aux_stat_val & 0x1;

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

    unsigned aux_stat_val = GET32(AUX_MU_STAT);
    unsigned done = (aux_stat_val & (0x1 << 9)) >> 9;

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

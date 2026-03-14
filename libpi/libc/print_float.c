#include "rpi.h"

/* Print a double value to stdout (via rpi_putchar). Uses simple fixed-point for fractional part. */
void print_float(double val) {
    if (val < 0) {
        rpi_putchar('-');
        val = -val;
    }
    unsigned long long int_part = (unsigned long long)val;
    double frac = val - (double)int_part;

    /* Print integer part */
    char buf[32];
    int i = 0;
    if (int_part == 0) {
        buf[i++] = '0';
    } else {
        char tmp[32];
        int n = 0;
        unsigned long long t = int_part;
        while (t > 0) {
            tmp[n++] = '0' + (t % 10);
            t /= 10;
        }
        while (n > 0) buf[i++] = tmp[--n];
    }
    for (int j = 0; j < i; j++) rpi_putchar(buf[j]);

    /* Print decimal point and up to 6 fractional digits */
    rpi_putchar('.');
    for (int j = 0; j < 6; j++) {
        frac *= 10;
        int d = (int)frac;
        rpi_putchar('0' + d);
        frac -= d;
    }
}

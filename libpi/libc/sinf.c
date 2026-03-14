#include "rpi.h"

/* sinf(x) - sine via range reduction + Taylor series */
float sinf(float x) {
    /* Reduce to [-pi, pi] */
    const float pi = 3.14159265358979323846f;
    const float two_pi = 6.28318530717958647692f;
    while (x > pi)  x -= two_pi;
    while (x < -pi) x += two_pi;

    /* Taylor: sin(x) = x - x³/6 + x⁵/120 - x⁷/5040 */
    float x2 = x * x;
    float x3 = x2 * x;
    float x5 = x3 * x2;
    float x7 = x5 * x2;
    return x - x3 * (1.0f / 6.0f) + x5 * (1.0f / 120.0f) - x7 * (1.0f / 5040.0f);
}

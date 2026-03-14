#include "rpi.h"

/* sqrtf(x) - square root via Newton-Raphson */
float sqrtf(float x) {
    if (x <= 0.0f) return 0.0f;
    /* Initial guess: use bit manipulation for fast approximation */
    union { float f; unsigned u; } u = { .f = x };
    u.u = (u.u >> 1) + (127 << 22); /* rough initial guess */
    float y = u.f;
    /* Newton-Raphson: y_new = 0.5 * (y + x/y). Two iterations for good accuracy. */
    y = 0.5f * (y + x / y);
    y = 0.5f * (y + x / y);
    return y;
}

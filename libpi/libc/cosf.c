#include "rpi.h"

/* cosf(x) - cosine via range reduction + Taylor series */
float cosf(float x) {
    /* Reduce to [-pi, pi] */
    const float pi = 3.14159265358979323846f;
    const float two_pi = 6.28318530717958647692f;
    while (x > pi)  x -= two_pi;
    while (x < -pi) x += two_pi;

    /* Taylor: cos(x) = 1 - x²/2 + x⁴/24 - x⁶/720 */
    float x2 = x * x;
    float x4 = x2 * x2;
    float x6 = x4 * x2;
    return 1.0f - x2 * 0.5f + x4 * (1.0f / 24.0f) - x6 * (1.0f / 720.0f);
}

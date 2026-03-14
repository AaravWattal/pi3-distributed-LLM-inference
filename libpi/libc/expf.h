#ifndef LIBPI_EXPF_H
#define LIBPI_EXPF_H

/* expf(x) - exponential. Range-reduced Taylor series approximation. */
static inline float expf(float x) {
    /* Clamp to avoid overflow - exp(88) ~ 1e38, near float max */
    if (x > 88.0f) return 1e38f;
    if (x < -88.0f) return 0.0f;

    /* exp(x) = 2^(k) * exp(r) where x = k*ln2 + r, |r| <= ln2/2 */
    const float ln2 = 0.69314718055994530942f;
    const float inv_ln2 = 1.44269504088896340736f;

    float kf = x * inv_ln2;
    int k = (int)(kf >= 0 ? kf + 0.5f : kf - 0.5f);
    float r = x - k * ln2;

    /* exp(r) ≈ 1 + r + r²/2 + r³/6 + r⁴/24 + r⁵/120 */
    float r2 = r * r;
    float r3 = r2 * r;
    float r4 = r3 * r;
    float r5 = r4 * r;
    float p = 1.0f + r + r2 * 0.5f + r3 * (1.0f / 6.0f) + r4 * (1.0f / 24.0f) + r5 * (1.0f / 120.0f);

    /* Scale by 2^k via ldexp-style */
    if (k >= 0) {
        float two_k = 1.0f;
        while (k > 0) { two_k *= 2.0f; k--; }
        return p * two_k;
    } else {
        float two_k = 1.0f;
        while (k < 0) { two_k *= 0.5f; k++; }
        return p * two_k;
    }
}

#endif

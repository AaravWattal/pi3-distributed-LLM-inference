#ifndef LIBPI_POWF_H
#define LIBPI_POWF_H

/* logf(x) - natural log. Used by powf. */
static inline float logf(float x) {
    if (x <= 0.0f) return -1e38f; /* invalid */
    /* log(x) = log(2) * log2(x). Use the identity log2(x) = exponent + log2(mantissa). */
    /* Simpler: Newton-Raphson for exp(y)=x => y = log(x). */
    /* Or polynomial approximation on [1,2]: log(x) for x in [1,2] */
    union { float f; unsigned u; } u = { .f = x };
    int exp_bits = ((u.u >> 23) & 0xFF) - 127;
    u.u = (u.u & 0x007FFFFF) | 0x3F800000; /* mantissa in [1,2) */
    float m = u.f;
    /* log(m) for m in [1,2): log(m) ≈ (m-1) - (m-1)²/2 + (m-1)³/3 - ... */
    float t = m - 1.0f;
    float t2 = t * t;
    float t3 = t2 * t;
    float log_m = t - t2 * 0.5f + t3 * (1.0f / 3.0f) - t3 * t * 0.25f;
    const float ln2 = 0.69314718055994530942f;
    return ln2 * (float)exp_bits + log_m;
}

/* powf(base, exp) = exp(exp * log(base)) */
static inline float powf(float base, float exp) {
    if (base <= 0.0f) return 0.0f;
    float log_b = logf(base);
    /* expf(exp * log_b) */
    float arg = exp * log_b;
    /* Use inline expf logic to avoid header dependency */
    if (arg > 88.0f) return 1e38f;
    if (arg < -88.0f) return 0.0f;
    const float ln2 = 0.69314718055994530942f;
    const float inv_ln2 = 1.44269504088896340736f;
    float kf = arg * inv_ln2;
    int k = (int)(kf >= 0 ? kf + 0.5f : kf - 0.5f);
    float r = arg - k * ln2;
    float r2 = r * r, r3 = r2 * r, r4 = r3 * r, r5 = r4 * r;
    float p = 1.0f + r + r2 * 0.5f + r3 * (1.0f / 6.0f) + r4 * (1.0f / 24.0f) + r5 * (1.0f / 120.0f);
    float two_k = 1.0f;
    if (k >= 0) { while (k > 0) { two_k *= 2.0f; k--; } }
    else { while (k < 0) { two_k *= 0.5f; k++; } }
    return p * two_k;
}

#endif

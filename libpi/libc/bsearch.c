#include "rpi.h"
#include <stddef.h>

/* bsearch - binary search. Standard C signature. */
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *)) {
    if (nmemb == 0) return NULL;
    const char *lo = (const char *)base;
    const char *hi = lo + (nmemb - 1) * size;

    while (lo <= hi) {
        const char *mid = lo + ((hi - lo) / (2 * size)) * size;
        int cmp = compar(key, mid);
        if (cmp < 0)
            hi = mid - size;
        else if (cmp > 0)
            lo = mid + size;
        else
            return (void *)mid;
    }
    return NULL;
}

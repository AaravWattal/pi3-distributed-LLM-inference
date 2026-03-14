#include "rpi.h"
#include <stddef.h>

static void swap(char *a, char *b, size_t size) {
    char tmp;
    for (; size > 0; size--, a++, b++) {
        tmp = *a;
        *a = *b;
        *b = tmp;
    }
}

static void qsort_impl(char *base, size_t nmemb, size_t size,
                       int (*compar)(const void *, const void *)) {
    if (nmemb <= 1) return;

    char *lo = base;
    char *hi = base + (nmemb - 1) * size;
    char *pivot = base + (nmemb / 2) * size;

    swap(pivot, hi, size);
    char *store = lo;
    for (char *p = lo; p < hi; p += size) {
        if (compar(p, hi) < 0) {
            swap(store, p, size);
            store += size;
        }
    }
    swap(store, hi, size);

    size_t left_n = (store - base) / size;
    size_t right_n = nmemb - left_n - 1;

    if (left_n > 1) qsort_impl(base, left_n, size, compar);
    if (right_n > 1) qsort_impl(store + size, right_n, size, compar);
}

/* qsort - quicksort. Uses recursion; stack usage is O(log n). */
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *)) {
    if (nmemb <= 1) return;
    qsort_impl((char *)base, nmemb, size, compar);
}

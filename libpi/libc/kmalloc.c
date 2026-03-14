/*
 * Simple bump allocator: no free, just have to reboot().
 * Uses __heap_start__ by default; kmalloc_init_set_start() can override.
 */
#include "rpi.h"
#include "memmap.h"

#define DEFAULT_HEAP_SIZE (4 * 1024 * 1024)  /* 4MB */

static uint8_t *heap_ptr;
static uint8_t *heap_end;
static uint8_t *heap_start;

void kmalloc_init_set_start(void *addr, unsigned max_nbytes) {
    heap_start = (uint8_t *)addr;
    heap_ptr = heap_start;
    heap_end = heap_start + max_nbytes;
}

void *kmalloc(unsigned nbytes) {
    if (!heap_ptr) {
        /* First use: default to __heap_start__ */
        heap_start = (uint8_t *)__heap_start__;
        heap_ptr = heap_start;
        heap_end = heap_start + DEFAULT_HEAP_SIZE;
    }
    nbytes = (nbytes + 3) & ~3;
    if (heap_ptr + nbytes > heap_end)
        return 0;
    void *p = heap_ptr;
    heap_ptr += nbytes;
    /* Zero-fill */
    for (unsigned i = 0; i < nbytes; i++)
        ((uint8_t *)p)[i] = 0;
    return p;
}

void *kmalloc_notzero(unsigned nbytes) {
    if (!heap_ptr) {
        heap_start = (uint8_t *)__heap_start__;
        heap_ptr = heap_start;
        heap_end = heap_start + DEFAULT_HEAP_SIZE;
    }
    nbytes = (nbytes + 3) & ~3;
    if (heap_ptr + nbytes > heap_end)
        return 0;
    void *p = heap_ptr;
    heap_ptr += nbytes;
    return p;
}

void *kmalloc_aligned(unsigned nbytes, unsigned alignment) {
    if (!heap_ptr) {
        heap_start = (uint8_t *)__heap_start__;
        heap_ptr = heap_start;
        heap_end = heap_start + DEFAULT_HEAP_SIZE;
    }
    uintptr_t addr = (uintptr_t)heap_ptr;
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    heap_ptr = (uint8_t *)aligned;
    nbytes = (nbytes + alignment - 1) & ~(alignment - 1);
    if (heap_ptr + nbytes > heap_end)
        return 0;
    void *p = heap_ptr;
    heap_ptr += nbytes;
    for (unsigned i = 0; i < nbytes; i++)
        ((uint8_t *)p)[i] = 0;
    return p;
}

void *kmalloc_heap_ptr(void) {
    return heap_ptr ? heap_ptr : (void *)__heap_start__;
}

void *kmalloc_heap_start(void) {
    return heap_start ? heap_start : (void *)__heap_start__;
}

void *kmalloc_heap_end(void) {
    return heap_end ? heap_end : (void *)((uint8_t *)__heap_start__ + DEFAULT_HEAP_SIZE);
}

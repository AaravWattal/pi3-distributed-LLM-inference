# Bare-Metal LLM on Raspberry Pi 3

Running a 15M-parameter language model on a Raspberry Pi 3 with no operating system, achieving **33 tokens/sec** from a baseline of **0.25 tokens/sec** (a 132x speedup).

For our CS 140E final project, we brought up a Raspberry Pi 3 from scratch, implementing GPIO, UART, and a bootloader. We then adapted Karpathy's [llama2.c](https://github.com/karpathy/llama2.c) to run on bare metal.

## Performance Summary

| Optimization Stage | Tokens/Sec | Speedup |
|---|---|---|
| Baseline | 0.25 | 1x |
| Multicore | 1 | 4x |
| Caching (dcache + icache) | 16 | 64x |
| Compiler Flags | 24 | 96x |
| `core_freq` 500 MHz | 31 | 124x |
| vectorized RMSNorm, attention, softmax | 33 | 132x |

## Optimizations

### Multicore Execution

The Pi 3 has four Arm Cortex-A53 cores. We wrote boot code to bring up all four cores and built an API for dispatching matrix-multiplication work across them with synchronization. This yielded the expected 4x speedup to **1 token/sec**, with little overhead due to the memory-bound nature of the workload at the time.

### Virtual Memory and Caching

We used the GPROF technique from labs to identify memory access as the bottleneck, then enabled dcache and icache. We set up virtual memory on ARMv7, and the result was a jump to **16 tokens/sec**. During this process, multicore execution caused garbage output, which we resolved by enabling hardware cache coherence.

### Compiler Optimizations

We experimented with GCC optimization flags to extract more performance. Enabling `-O3` raised throughput to **22 tokens/sec**, and adding `-ffast-math` brought it to **24 tokens/sec**.

### Clock Frequency Bump

The Pi 3's `core_freq` parameter controls the GPU and L2 cache clock, defaulting to 500 MHz. However, our use of mini-UART locked it to 250 MHz. We switched to PL011 UART, which has an independent clock source, and raised `core_freq` back to 500 MHz. This doubled the L2 cache bandwidth and brought throughput to **31 tokens/sec**.

### Unsuccessful Optimizations

We also explored several optimizations that did not yield measurable gains:

- **NEON SIMD instructions** — no effect, since the compiler was already emitting vector floating-point operations.
- **Software cache prefetching** — no improvement.
- **Precomputing RoPE frequencies** — no improvement.
- **Tiled matrix multiplication** — rewriting for cache-friendliness did not help.
- **Vectorized RMSNorm, Softmax, and attention** — minor performance gains.

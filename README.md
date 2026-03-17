# llama-pi3

Single core: 10 tok/sec
Multicore: 16 tok/sec
Cache prefetch: 16 tok/sec (negligible)
-O3 flag: 22 tok/sec
-ffast-math: 24 tok/sec
faster core_freq=400 (need uart): 27.9 tok/sec
faster core_freq=500 (need uart): 31.8 tok/sec

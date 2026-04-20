# Technique: False Sharing

## What It Measures

Cache coherency overhead when multiple threads write to distinct variables that happen to reside on the same 64-byte cache line. The MESI protocol forces every write to invalidate the line on other cores, serializing what should be independent work.

## Why It Matters

In multi-threaded code, packing per-thread state (counters, accumulators, flags) naively into an array causes invisible contention. The threads appear independent but the hardware is not.

## How To Run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target bench_false_sharing
./build/techniques/false_sharing/bench_false_sharing --benchmark_repetitions=5
```

## Results

```
CPU:      Apple M1 Pro (8 physical cores)
L1d:      64 KiB  L2: 4096 KiB  L3: N/A (unified L2)
OS:       macOS 15.7.4
Compiler: Homebrew clang 20.1.8
Flags:    -O2 -march=native
Threads:  4  Iterations per thread: 10,000,000
Date:     2026-04-20
```

| Benchmark         | Median (ms) | Notes                      |
|-------------------|-------------|----------------------------|
| BM_FalseSharing   | 588         | 4 threads, packed counters |
| BM_NoFalseSharing | 43.9        | 4 threads, 64B padded      |
| **Speedup**       | **~13x**    |                            |

## Conclusion / Takeaway

Pad per-thread hot data to `alignas(64)` (one cache line). Cost is a bit of memory; benefit is near-linear multi-core scaling. Even a single extra `char pad[]` suffices.

## Further Reading

- [What Every Programmer Should Know About Memory](https://people.freebsd.org/~lstewart/articles/cpumemory.pdf) — Ulrich Drepper, §3.3.4
- [false sharing — cppreference](https://en.cppreference.com/w/cpp/thread/hardware_destructive_interference_size)
- `std::hardware_destructive_interference_size` (C++17): portable cache line size constant

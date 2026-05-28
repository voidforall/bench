# Technique: Slowpath Removal

## What It Measures

The cost of letting error-handling code pollute the hot path — both by occupying
instruction-cache lines and by placing the common case at the tail of a chain of
conditional checks.

## Why It Matters

In tight loops (order validation, packet processing, game-logic tick), the common
path is "no error." Chained `if/else` puts `sendOrder` at the very end, and inline
handlers physically live inside the loop's code, expanding its instruction footprint.
Even when an error never fires, the CPU must fetch and decode those extra instructions.

The fix has two parts:
1. **Accumulate conditions into flags** — single `if (!flags)` branch for the hot path.
2. **Keep error handlers `[[noinline]]`** — they live outside the hot loop's code pages.

## How To Run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target bench_slowpath_removal
./build/techniques/slowpath_removal/bench_slowpath_removal --benchmark_repetitions=5
```

Use `perf stat -e L1-icache-load-misses` to observe instruction-cache effects directly.

## Benchmarks

| Benchmark                 | Pattern                              | Handler placement |
|---------------------------|--------------------------------------|-------------------|
| BM_ChainedInlineHandlers  | chained if/else — hot path at end    | always_inline     |
| BM_ChainedNoinlineHandlers| chained if/else — hot path at end    | noinline          |
| BM_FlagsNoinlineHandler   | flags accumulation — single branch   | noinline          |

Setup: 16 K orders, 1% error rate per condition (three independent checks).

## Results

```
CPU:      Apple M1 Pro (8 physical cores)
L1i:      128 KiB  L2: 4096 KiB  L3: N/A (unified L2)
OS:       macOS 15.7.4
Compiler: Homebrew clang 20.1.8
Flags:    -O2 -march=native
Orders:   16,384  |  error rate: 1% per condition
Date:     2026-05-28
```

| Benchmark                  | Median (ns) | Throughput     | Notes                                        |
|----------------------------|-------------|----------------|----------------------------------------------|
| BM_ChainedInlineHandlers   | 12,408      | 1.33 G items/s | 3 handler bodies inlined into loop           |
| BM_ChainedNoinlineHandlers | 11,453      | 1.44 G items/s | handlers out-of-line, still 3 branches       |
| BM_FlagsNoinlineHandler    | 10,484      | 1.56 G items/s | OR-reduce + single branch, handler noinline  |
| **Speedup (flags vs inline)**| **~1.18x** |                | flags beats chained-inline                   |

## Conclusion / Takeaway

1. **Move the hot path to the top** — `if (!errorFlags)` fires 97%+ of the time.
2. **Always mark error handlers `[[gnu::noinline]]`** — keeps them out of the loop's
   instruction footprint even if the compiler's inlining heuristic would inline them.
3. **OR-reduce error conditions into a single flags word** — branchless accumulation
   + one predictable branch beats a chain of N branches.

## Further Reading

- [CppCon 2014: Mike Acton "Data-Oriented Design"](https://www.youtube.com/watch?v=rX0ItVEVjHc) — code layout and cache discipline
- Agner Fog's *Optimizing Software in C++*, ch. 11 — instruction cache optimisation
- `[[likely]]` / `[[unlikely]]` (C++20) — compiler hint to place cold code away from hot path

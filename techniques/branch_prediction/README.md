# Technique: Branch Prediction

## What It Measures

CPU pipeline stall cost when the branch predictor cannot correctly anticipate conditional jumps. A mispredicted branch flushes the pipeline (typically 15–20 cycle penalty on modern x86).

## Why It Matters

A tight loop with a data-dependent conditional can be 3–8x slower on random data vs. sorted data — purely due to branch mispredictions, not work done. Going branchless eliminates the penalty entirely.

## How To Run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target bench_branch_prediction
./build/techniques/branch_prediction/bench_branch_prediction --benchmark_repetitions=5
```

Use `perf stat -e branch-misses` to see misprediction rates directly.

## Results

```
CPU:      Apple M1 Pro (8 physical cores)
L1d:      64 KiB  L2: 4096 KiB  L3: N/A (unified L2)
OS:       macOS 15.7.4
Compiler: Homebrew clang 20.1.8
Flags:    -O2 -march=native
Array:    65,536 int elements
Date:     2026-04-20
```

| Benchmark        | Median (ns) | Throughput     | Notes                                        |
|------------------|-------------|----------------|----------------------------------------------|
| BM_UnsortedSum   | 64,020      | 1.03 G items/s | shuffled; ARM predictor handles it well      |
| BM_SortedSum     | 64,187      | 1.03 G items/s | sorted; ≈ same as unsorted on M1             |
| BM_BranchlessSum | 34,030      | 1.96 G items/s | `x * (x > threshold)`; auto-vectorized       |
| **Speedup**      | **~1.9x**   |                | branchless vs unsorted                       |

> **Note (Apple Silicon):** On M1/M2 the branch predictor is strong enough that sorted ≈ unsorted for this 50/50 pattern. The branchless version still wins ~2x because clang vectorizes `int * bool` into SIMD. On x86 with a weaker predictor, sorted typically beats unsorted by 3–8x.

## Conclusion / Takeaway

1. **Sort data** when the branch distribution is skewed — predictor learns the pattern.
2. **Go branchless** (`val * (cond)`) when sorting is too expensive or data is truly random.
3. Compiler flag `-O2` may auto-vectorize the branchless version further.

## Further Reading

- [Why is processing a sorted array faster?](https://stackoverflow.com/questions/11227809) — classic SO answer
- [Agner Fog's optimization manuals](https://www.agner.org/optimize/) — microarchitecture details
- GCC intrinsic: `__builtin_expect(cond, likely_val)` to hint the predictor

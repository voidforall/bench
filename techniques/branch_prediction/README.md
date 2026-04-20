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

> Fill in after running on your machine. See `docs/hardware.md` for the template.

| Benchmark        | Time (ns/iter) | Items/s | Notes               |
|------------------|----------------|---------|---------------------|
| BM_UnsortedSum   | ???            | ???     | ~50% misprediction  |
| BM_SortedSum     | ???            | ???     | predictor succeeds  |
| BM_BranchlessSum | ???            | ???     | cmov, no branch     |

## Conclusion / Takeaway

1. **Sort data** when the branch distribution is skewed — predictor learns the pattern.
2. **Go branchless** (`val * (cond)`) when sorting is too expensive or data is truly random.
3. Compiler flag `-O2` may auto-vectorize the branchless version further.

## Further Reading

- [Why is processing a sorted array faster?](https://stackoverflow.com/questions/11227809) — classic SO answer
- [Agner Fog's optimization manuals](https://www.agner.org/optimize/) — microarchitecture details
- GCC intrinsic: `__builtin_expect(cond, likely_val)` to hint the predictor

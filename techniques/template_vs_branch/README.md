# Technique: Prefer Templates to Branches

## What It Measures

The cost of a runtime `Side` parameter that flows through every helper function
vs. a compile-time `Side` template parameter that eliminates dead code at
instantiation.

## Why It Matters (x86 context)

In the classic HFT pattern, strategy helpers like `CalcPrice`, `CheckRiskLimits`,
and `SendOrder` each branch on `Side`. With a runtime parameter these branches
accumulate, causing mispredictions and preventing auto-vectorization. Templating
on `Side` lifts the parameter to compile time — `if constexpr` paths for the
unused side are removed entirely, leaving straight-line arithmetic with zero
branch overhead per order.

```cpp
// Branching: each helper branches at runtime
float CalcPrice(Side s, float fair, float credit) {
    return s == Side::Buy ? fair - credit : fair + credit;
}

// Template: compiler generates two separate functions, dead branch removed
template <Side S>
float CalcPrice(float fair, float credit) {
    if constexpr (S == Side::Buy) return fair - credit;
    else                          return fair + credit;
}
```

## Benchmarks

| Benchmark              | Stream          | Dispatch                       |
|------------------------|-----------------|--------------------------------|
| BM_Branch_Homogeneous  | all Buy         | 3× runtime ternary per order   |
| BM_Template_Homogeneous| all Buy         | zero branches, Buy path only   |
| BM_Branch_Mixed        | 50/50 Buy/Sell  | 3× runtime ternary per order   |
| BM_Template_Mixed      | 50/50 Buy/Sell  | 1 dispatch branch + 0 inside   |

## How To Run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target bench_template_vs_branch
./build/techniques/template_vs_branch/bench_template_vs_branch --benchmark_repetitions=5
```

## Results

```
CPU:      Apple M1 Pro (8 physical cores)
L1i:      128 KiB  L2: 4096 KiB  L3: N/A (unified L2)
OS:       macOS 15.7.4
Compiler: Homebrew clang 20.1.8
Flags:    -O2 -march=native
Orders:   16,384  |  50/50 Buy/Sell for mixed stream
Date:     2026-05-28
```

| Benchmark               | Median (ns) | Throughput     | Notes                                        |
|-------------------------|-------------|----------------|----------------------------------------------|
| BM_Branch_Homogeneous   | 15,583      | 1.06 G items/s | 3× fcsel; auto-vectorized by clang           |
| BM_Template_Homogeneous | 15,514      | 1.06 G items/s | same SIMD output; ≈ identical to branch      |
| BM_Branch_Mixed         | 26,545      | 621 M items/s  | fcsel has no misprediction penalty on M1     |
| BM_Template_Mixed       | 31,266      | 528 M items/s  | two instantiations → more I-cache churn      |
| **Speedup (branch vs template, mixed)** | **~1.18× in favour of branch** | | |

## Conclusion / Takeaway

1. **Homogeneous streams**: template ≈ branch on M1. Clang auto-vectorizes the
   branching loop using `fcsel` (floating-point conditional select) — a single
   instruction with no misprediction cost. Both variants emit near-identical SIMD.

2. **Mixed 50/50 streams**: branching is ~18% *faster* on M1 because it stays in
   one code stream (I-cache friendly), while the template version context-switches
   between two instantiations.

3. **The advice is x86-centric.** On x86 with a weaker predictor (15–20 cycle
   mispredict penalty) and without an equivalent of `fcsel`, templating out the
   `Side` parameter eliminates real branch cost. On ARM, `fcsel`/`csel` make the
   branching version equally cheap. For mixed-side traffic on ARM, templates can
   *hurt* due to I-cache fragmentation.

4. **Templates still win when**: the side is truly fixed at compile time (a
   dedicated Buy-only binary), or the two instantiations have fundamentally
   different algorithms (enabling SIMD in one path that branches would block).

## Further Reading

- Agner Fog's *Optimizing Software in C++*, ch. 14 — branch vs. conditional move
- [ARM ISA Reference: FCSEL](https://developer.arm.com/documentation/) — single-cycle conditional float select
- CppCon 2015: Chandler Carruth "Tuning C++: Benchmarks, and CPUs, and Compilers!" — why microbenchmarks differ across ISAs

# Technique: Prefer Templates to Branches

## What It Measures

The cost of a runtime `Side` parameter that flows into every helper function vs.
a compile-time `Side` template parameter that removes the unused code path entirely
at instantiation.

## Why It Matters

In the HFT pattern from the screenshot, helpers like `CalcPrice`, `CheckRiskLimits`,
and `SendOrder` each branch on `Side`. When `Side` is a runtime value, every call
computes *both* the Buy and Sell result and then selects — even if only one path is
ever needed. With a template parameter the dead path is eliminated by `if constexpr`,
leaving straight-line arithmetic with no conditional overhead.

```cpp
// Branching: runtime Side → compiler computes both paths + fcsel every call
[[gnu::noinline]] float calc_price(Side s, float fair, float credit) {
    if (s == Side::Buy) return (fair - credit) * (1.0f - credit * 0.001f);
    return                     (fair + credit) * (1.0f + credit * 0.0015f);
}
// ~13 instructions per call: buy-path ops + sell-path ops + cmp + fcsel

// Template: compile-time Side → only the live path survives
template <Side S>
[[gnu::noinline]] float calc_price_t(float fair, float credit) {
    if constexpr (S == Side::Buy) return (fair - credit) * (1.0f - credit * 0.001f);
    else                          return (fair + credit) * (1.0f + credit * 0.0015f);
}
// ~7 instructions per call: single-path arithmetic, no conditional
```

### Why the first version of this benchmark was wrong

The original helpers used simple ternaries (`a - b : a + b`). Clang converts those
to `fcsel` (ARM floating-point conditional select), which evaluates both sides but
has zero branch-prediction cost, making both variants generate nearly identical code.
The fix: `[[gnu::noinline]]` on every helper forces real call boundaries that prevent
the compiler from fusing or hoisting logic, and multi-operation paths make eager
evaluation of both sides more expensive than just picking one.

## Benchmarks

| Benchmark               | Stream         | Helper dispatch                          |
|-------------------------|----------------|------------------------------------------|
| BM_Branch_Homogeneous   | all Buy        | 3× runtime helpers, each ~13 instructions |
| BM_Template_Homogeneous | all Buy        | 3× specialised helpers, each ~7 instructions, zero conditional ops |
| BM_Branch_Mixed         | 50/50 Buy/Sell | same 3× runtime helpers                 |
| BM_Template_Mixed       | 50/50 Buy/Sell | 1 dispatch branch + 3× branch-free helpers |

## How To Run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target bench_template_vs_branch
./build/techniques/template_vs_branch/bench_template_vs_branch --benchmark_repetitions=9 --benchmark_min_time=1s
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

| Benchmark               | Median (ns) | Throughput     | Notes                                         |
|-------------------------|-------------|----------------|-----------------------------------------------|
| BM_Branch_Homogeneous   | 41,501      | 395 M items/s  | both paths computed + fcsel per call          |
| BM_Template_Homogeneous | 36,172      | 454 M items/s  | single-path per call; **1.15x faster**        |
| BM_Branch_Mixed         | 109,157     | 154 M items/s  | 2.6× slower than homogeneous branch           |
| BM_Template_Mixed       | 75,986      | 218 M items/s  | 1 dispatch branch + branch-free calls; **1.44x faster than branch mixed** |

## Conclusion / Takeaway

1. **Instruction count, not misprediction, is the primary cost.** Even without real
   branches (clang uses `fcsel` for simple conditionals), branching helpers execute
   ~13 instructions per call while template helpers execute ~7. With 3 helpers per
   order across 16 K orders that is 288 K extra instructions per benchmark iteration.

2. **Templates win on both homogeneous and mixed streams** once helper boundaries
   are opaque to the compiler (`[[gnu::noinline]]`). The homogeneous gain is ~15%;
   the mixed gain is ~44% because the data-dependent load of `o.side` on every
   helper call also disrupts the superscalar pipeline.

3. **The real-world pattern**: mark strategy helpers `[[gnu::noinline]]` to prevent
   the optimizer from re-merging your template specialisations back into a generic
   function. Without `noinline`, both approaches collapse to the same code.

4. **Mixed streams still benefit from templates** because the dispatch branch
   (one per order, at the call site) is cheaper than three internal fcsel chains
   (one per helper call). For maximum throughput, batch orders by side before
   dispatching so the inner loop has zero conditional ops.

## Further Reading

- Agner Fog's *Optimizing Software in C++*, ch. 14 — branch vs. conditional move
- [ARM ISA: FCSEL](https://developer.arm.com/documentation/) — why simple conditionals become register-selects
- CppCon 2015: Chandler Carruth — "Tuning C++: Benchmarks, and CPUs, and Compilers!"

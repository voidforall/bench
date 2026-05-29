# bench

C++ performance tuning techniques with microbenchmarks.

Each technique lives under `techniques/` with its own benchmark code and a README explaining the concept, how to reproduce the numbers, and the takeaway.

## Techniques

| Technique | Topic | Key Takeaway | Typical Speedup |
|-----------|-------|--------------|-----------------|
| [false_sharing](techniques/false_sharing/) | Cache coherency | Pad per-thread data to 64B cache line | ~13x |
| [branch_prediction](techniques/branch_prediction/) | CPU pipeline | Go branchless; sort only helps on x86 | ~2x (ARM), ~8x (x86) |
| [small_data_lookup](techniques/small_data_lookup/) | Data structure selection | HashMap beats linear/binary for int keys at all small N | flat ~2ns vs O(N) |
| [string_view_vs_string](techniques/string_view_vs_string/) | Allocation elimination | Use string_view for read-only params; substr is free | 15–160x (pass), 65x (substr) |
| [slowpath_removal](techniques/slowpath_removal/) | Code layout / I-cache | Accumulate error flags + noinline handlers; single hot-path branch | ~1.18x |
| [template_vs_branch](techniques/template_vs_branch/) | Compile-time dispatch | noinline helpers expose real cost: template saves ~6 instructions/call vs branching's eager-eval-both-paths | 1.15x (homogeneous), 1.44x (mixed) |
| [denormalized_lookup](techniques/denormalized_lookup/) | Data layout | Inline the joined field — skip the dependent load; flat ~1.25 ns/op vs normalized's degradation past L2 | 1.5x at 32 MiB working set |
| [cache_priming](techniques/cache_priming/) | I/D-cache + branch predictor | Run dummies through the same pipeline; rare real events land on a warm cache | 8.9x (cold→primed), within 8% of hot baseline |

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Requires: CMake ≥ 3.20, C++17 compiler, internet access (fetches [google/benchmark](https://github.com/google/benchmark) v1.9.1).

## Run a Specific Benchmark

```bash
./build/techniques/false_sharing/bench_false_sharing --benchmark_repetitions=5
./build/techniques/branch_prediction/bench_branch_prediction --benchmark_repetitions=5
```

## Run All Benchmarks

```bash
bash scripts/run_all.sh
# results saved to results/
```

## Adding a New Technique

See [docs/adding_technique.md](docs/adding_technique.md).

## Measurement Methodology

See [docs/methodology.md](docs/methodology.md) for how to reduce noise, pin CPUs, and record reproducible results.

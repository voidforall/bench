# bench

C++ performance tuning techniques with microbenchmarks.

Each technique lives under `techniques/` with its own benchmark code and a README explaining the concept, how to reproduce the numbers, and the takeaway.

## Techniques

| Technique | Topic | Key Takeaway | Typical Speedup |
|-----------|-------|--------------|-----------------|
| [false_sharing](techniques/false_sharing/) | Cache coherency | Pad per-thread data to 64B cache line | 5–10x |
| [branch_prediction](techniques/branch_prediction/) | CPU pipeline | Sort data or use branchless ops | 3–8x |

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

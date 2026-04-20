# Benchmarking Methodology

## Build Flags

Always build in Release with native tuning:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O2 -march=native"
cmake --build build -j
```

Never benchmark a Debug build.

## Reducing Noise

| Source | Mitigation |
|--------|-----------|
| CPU frequency scaling | `sudo cpupower frequency-set -g performance` (Linux) |
| Turbo Boost | Disable in BIOS or via `/sys/devices/system/cpu/intel_pstate/no_turbo` |
| ASLR | `echo 0 \| sudo tee /proc/sys/kernel/randomize_va_space` (Linux) |
| SMT / Hyperthreading | Pin threads to physical cores: `taskset -c 0,2,4,6 ./bench_*` |
| OS jitter | Use `--benchmark_repetitions=10` and report median, not mean |

## Preventing Dead Code Elimination

Always use `benchmark::DoNotOptimize(val)` and `benchmark::ClobberMemory()` to prevent the compiler from optimizing away the code under test.

## Measuring with perf (Linux)

```bash
perf stat -e cycles,instructions,cache-misses,branch-misses ./bench_* --benchmark_min_time=2.0
```

## Reporting Results

Fill in the Results table in each technique's `README.md` with:
- Hardware (CPU model, core count, L1/L2/L3 cache sizes)
- OS and kernel version
- Compiler and version (`g++ --version`)
- Exact flags used
- Raw numbers (median across repetitions)

See `docs/hardware.md` for the template.

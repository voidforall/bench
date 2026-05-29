# Technique: Cache Priming via Dummy Traffic

## What It Measures

The latency cost of a "real" event arriving after the system has been idle long enough for the caches to go cold — and how much of that cost can be reclaimed by running representative *dummy* events through the same pipeline while waiting.

The talk's pattern (paraphrasing):

> Market data decoder ─► Strategy ─► Execution engine ─► (suppress send for dummies, fire for reals)
>
> Run dummies frequently through the entire pipeline. The single real event of the hour shares the same code path and the same data structures, so it lands on caches the dummies have just primed.

The benchmark times the latency of one batch of real events under three conditions:

1. **Cold** — pollute the cache with a 16 MiB scan, then measure the real batch.
2. **WarmedByDummy** — pollute, run a dummy batch (untimed), then measure the real batch.
3. **HotBaseline** — no pollute, after warmup. Floor.

If the technique works, condition 2 should approach condition 3.

## Why It Matters

In a market-making or HFT system, real signals are rare and latency-critical. Between them, the OS, neighbor processes, and even your own background work evict your hot lines. By the time the real signal lands, the codepath is in DRAM and the branch predictor has forgotten which way you usually go. That first event eats a fat latency penalty exactly when it matters most.

Dummies trade throughput for tail latency: you spend some cycles on fake events so the real one runs in cache. In a system that's idle 99% of the time at the message level but loaded continuously at the dummy level, the trade is essentially free — those cycles were going to waste anyway.

## How To Run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target bench_cache_priming
./build/techniques/cache_priming/bench_cache_priming \
  --benchmark_repetitions=5 --benchmark_min_time=0.2s \
  --benchmark_report_aggregates_only=true
```

Heads-up: don't use `--benchmark_min_time=1s` with `--benchmark_repetitions>=10`. Each iteration costs ~300 μs of untimed setup (polluter + maybe dummy) but only ~6 μs of measured time, so google-benchmark inflates iteration counts and the whole sweep can take 20+ minutes.

## Results

```
CPU:      Apple M1 Pro (8 cores)
L1d:      64 KiB    L2: 4 MiB (unified)
OS:       macOS 15.7.4 (Darwin 24.6.0)
Compiler: Apple clang
Flags:    -O2 -march=native
Workload: 512-message batch per measurement; pollute = 16 MiB scan
Date:     2026-05-29
```

| Treatment             | Per-batch (512 msgs) | Per-message | vs Cold       | vs Hot      |
|-----------------------|---------------------:|------------:|--------------:|------------:|
| Cold (just polluted)  | 6429 ns              | 12.6 ns     | 1.00x         | 9.6x slower |
| **WarmedByDummy**     | **723 ns**           | **1.4 ns**  | **8.9x faster** | 1.08x slower |
| HotBaseline           | 670 ns               | 1.3 ns      | 9.6x          | 1.00x       |

The dummy recovers 92% of the cold-cache penalty and lands within 8% of the never-evicted baseline.

## Methodology Notes

A few things worth knowing if you want to extend or trust this benchmark:

- **`UseManualTime` is mandatory.** The polluter and dummy passes are *not* what we're trying to measure — they're the cost we accept system-side to get fast reals. We wrap `std::chrono::steady_clock` around the real batch only.
- **`__attribute__((noinline))` on `pipeline_step` is load-bearing.** Without it, the compiler specializes the `dummy` flag at every call site and the dummy + real paths end up at different addresses → the dummy's i-cache warming work doesn't transfer.
- **Per-batch timing, not per-message.** A single pipeline call is ~1.3 ns hot, which is below the macOS clock floor (~42 ns per mach tick). Batching 512 messages lifts the timed region to microseconds — easily above the clock resolution — while keeping each cacheline touched once per batch, preserving the cold-vs-warm contrast.
- **The dummy uses the *same* messages as the real batch.** This is the perfect-warming upper bound. In production, dummies cover the *structural* paths (decoder table, branch predictor, code) but can't pre-fetch every instrument a real message might hit. Expect the real-world recovery to be somewhere between 50% and 90% rather than the 92% measured here.
- **i-cache vs d-cache.** The 16 MiB polluter scan doesn't displace pipeline code (modern i-caches are big), so most of the measured cold penalty here is d-cache reload. In a real system the OS/JIT/other processes evict i-cache too, and the dummies help there as well — the technique compounds.

## Conclusion / Takeaway

**Idle hurts more than work.** A cold cache costs ~9x on the first real event versus a hot one. Running dummies through the same pipeline closes that gap to within 8% of the hot floor.

When to apply:
- You have a rare-but-critical event class on a long-running, mostly-idle process (HFT signals, oncall pages, fault-handlers, periodic compactions).
- The pipeline is non-trivial — several stages, multiple lookup tables, branch-heavy dispatch. (For a 5-line hot loop, just keep it warm with real traffic.)
- You can cleanly distinguish dummy from real with a single late-stage flag, so dummy and real share the same code and data accesses up until the very end.

When *not* to:
- The hot path is so small it fits in L1 and stays there. Dummies are pure waste.
- The dummy and real paths diverge early. You'd be warming a different code than the one that runs.
- The downstream system can't safely ignore dummy outputs. The suppression must be 100% bulletproof — a dummy that accidentally hits the exchange is much worse than a cold-cache miss.

Bonus, as the slide notes: this also trains the hardware branch predictor on the real distribution of branches. The cache effect is most of the win on the M1 numbers above; on x86 with aggressive speculation, the branch-predictor effect can be comparable in magnitude.

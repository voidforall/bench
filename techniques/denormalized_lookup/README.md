# Technique: Denormalized Lookup (Inline the Join)

## What It Measures

The cost of joining two tables on the hot path versus duplicating the joined field into the larger table so the read needs only one cacheline.

Workload: per-order notional calculation `price * qty * quantityMultiplier`, repeated over a stream of orders that touch instruments in random order. Two layouts of the same data:

```cpp
// Normalized (the textbook design)
struct Market     { int32_t id; char shortName[4]; int16_t quantityMultiplier; ... };
struct Instrument { float price; int32_t marketId; ... };

notional = ins.price * qty * Markets[ins.marketId].quantityMultiplier;
//                            └──── extra load, dependent on the first ────┘

// Denormalized (copy the field that's hot)
struct Instrument { float price; int16_t quantityMultiplier; int32_t marketId; ... };

notional = ins.price * qty * ins.quantityMultiplier;
//                            └─ same cacheline as ins.price ─┘
```

Both structs are padded to 64 B (one cacheline). The arithmetic is identical; the only difference is whether the multiplier comes free with the instrument load or requires a second, dependent load into the markets table.

We sweep N from 128 to 262144 instruments (with N markets each), spanning working sets from 16 KiB (fits L1) to 32 MiB (spills DRAM).

## Why It Matters

The "normalize everything, look up at read time" advice comes from databases, where storage is the dominant cost. On the hot path of a latency-sensitive system the math flips:

- The second load is **dependent** on the first (`ins.marketId` is the address for the market load). The CPU can't issue them in parallel.
- Random access means hardware prefetchers can't hide the latency.
- Duplicating an `int16_t` per instrument costs 2 bytes; missing L2 once costs ~14 cycles.

So denormalization trades a small amount of memory and a synchronization burden (keep the copies in sync when a market's multiplier changes — rare in this domain) for a guaranteed single-cacheline read on every order.

## How To Run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target bench_denormalized_lookup
./build/techniques/denormalized_lookup/bench_denormalized_lookup \
  --benchmark_repetitions=5 --benchmark_report_aggregates_only=true
```

## Results

```
CPU:      Apple M1 Pro (8 cores)
L1d:      64 KiB    L2: 4 MiB (unified)
OS:       macOS 15.7.4 (Darwin 24.6.0)
Compiler: Apple clang
Flags:    -O2 -march=native
Workload: 4096 random index ops per benchmark iteration, median of 5 reps
Date:     2026-05-28
```

Per-op time (ns), divided out from the 4096-op inner loop:

| N         | Working set (norm) | Normalized | Denormalized | Speedup |
|-----------|--------------------|-----------:|-------------:|--------:|
| 128       | 16 KiB (L1)        | 1.27 ns    | **1.25 ns**  | 1.02x   |
| 1024      | 128 KiB (L2)       | 1.25 ns    | **1.24 ns**  | 1.01x   |
| 16384     | 2 MiB (L2)         | 1.33 ns    | **1.25 ns**  | 1.06x   |
| 262144    | 32 MiB (DRAM)      | 1.89 ns    | **1.26 ns**  | **1.50x** |

The denormalized layout stays flat at ~1.25 ns/op across all sizes. The normalized layout matches it while everything fits in cache, then degrades by 50% once the tables spill past L2.

## Conclusion / Takeaway

**Denormalized data is not a sin on the hot path.** The textbook diagram (one Market row, every Instrument references it by id) is the right design for a database. On the read side of a low-latency system, it costs you a dependent load on every operation — invisible when everything is hot, expensive when it isn't.

When to denormalize:
- The joined field is read on the hot path and written rarely (multipliers, currency codes, tick sizes).
- The duplicated field is small enough to inline (a few bytes) — large blobs are different.
- You have a clear plan to invalidate the copies when the source changes (an update path, even if cold, is required).

When not to:
- Field changes frequently or has many consumers that all need a consistent snapshot.
- Field is large enough that duplicating it inflates the hot struct out of one cacheline.
- The lookup table is tiny and provably stays in L1 forever — then the dependent load is cheap and you keep the storage savings.

The benchmark shows the cost of the lookup is **not zero just because the tables are small**. It's zero only while they're hot. Plan for the case where they're not.

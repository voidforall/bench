# Technique: Small Data Lookup

## What It Measures

Search performance across three strategies — linear scan, binary search, and hash map — as collection size grows from 4 to 128 elements.

All three structures fit comfortably in L1/L2 cache at these sizes, so the comparison isolates algorithmic overhead and constant factors rather than memory latency.

## Why It Matters

O(1) and O(log n) complexity analysis can mislead at small N. Hash maps carry per-lookup overhead (hash computation, bucket indirection, possible collision chain). Binary search pays branch mispredictions at each comparison step. For tiny collections, a simple `std::find` over a contiguous array may win due to cache friendliness and zero setup cost.

The crossover point matters a lot in hot paths: dispatcher tables, config lookups, small sets of options.

## How To Run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target bench_small_data_lookup
./build/techniques/small_data_lookup/bench_small_data_lookup --benchmark_repetitions=5
```

## Results

```
CPU:      Apple M1 Pro (8 physical cores)
L1d:      64 KiB  L2: 4096 KiB  L3: N/A (unified L2)
OS:       macOS 15.7.4
Compiler: Homebrew clang 20.1.8
Flags:    -O2 -march=native
Workload: 50% hits / 50% misses, alternating
Date:     2026-04-20
```

| N   | LinearSearch (ns) | BinarySearch (ns) | HashMap (ns) | Winner   |
|-----|-------------------|-------------------|--------------|----------|
| 4   | 2.35              | 2.63              | **1.79**     | HashMap  |
| 8   | 3.27              | 3.30              | **2.13**     | HashMap  |
| 16  | 4.40              | 3.77              | **1.85**     | HashMap  |
| 32  | 6.44              | 4.26              | **2.15**     | HashMap  |
| 64  | 12.0              | 4.72              | **2.14**     | HashMap  |
| 128 | 24.0              | 5.19              | **2.19**     | HashMap  |

## Conclusion / Takeaway

**HashMap wins at every size** — which contradicts the common advice "use linear search for small N."

Why:
- Integer hash is trivial (identity or a few bit-ops); hashing cost is negligible
- `reserve(n)` pre-allocates buckets, so all buckets fit in L1 at these sizes → lookup is ~2 cache accesses regardless of N
- HashMap time is essentially **flat** (1.8–2.2 ns) while linear scales as O(N) and binary as O(log N)

Crossover intuition breaks down when:
1. The key type is cheap to hash (integers, pointers)
2. The map is pre-warmed in cache
3. Load factor is kept low via `reserve`

**When linear scan can still win:** non-integer keys with expensive hash functions, or when the vector is constructed on-the-fly and the map's heap allocation dominates.

## Further Reading

- [When is a hash table better than an array?](https://stackoverflow.com/questions/3949217) 
- [std::find vs std::binary_search performance](https://www.modernescpp.com/index.php/linear-search-vs-binary-search)
- [Cache-friendly data structures](https://www.youtube.com/watch?v=WDIkqP4JbkE) — CppCon talk by Chandler Carruth

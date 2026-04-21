# Technique: string_view vs string

## What It Measures

Three scenarios where `std::string_view` eliminates heap allocation and copying compared to `std::string`:

1. **Argument passing** — read-only function receiving a string by value vs. `string_view`
2. **Substring extraction** — `string::substr` (allocates) vs. `string_view::substr` (pointer + length)
3. **Building a collection of substrings** — allocation pressure across many slices

## Why It Matters

`std::string` owns its data. Copying or slicing it triggers heap allocation. `std::string_view` is a non-owning `(ptr, length)` pair — construction, copy, and `substr` are all free. Every read-only string API that accepts `const std::string&` is a missed opportunity.

## How To Run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target bench_string_view_vs_string
./build/techniques/string_view_vs_string/bench_string_view_vs_string --benchmark_repetitions=5
```

## Results

```
CPU:      Apple M1 Pro (8 physical cores)
L1d:      64 KiB  L2: 4096 KiB  L3: N/A (unified L2)
OS:       macOS 15.7.4
Compiler: Homebrew clang 20.1.8
Flags:    -O2 -march=native
Date:     2026-04-21
```

### 1. Argument Passing (read-only function)

| Benchmark           | Median (ns) | Notes                        |
|---------------------|-------------|------------------------------|
| BM_PassString_Long  | 52.1        | 256-byte string → heap alloc + memcpy |
| BM_PassView_Long    | **0.32**    | pointer + length, zero copy  |
| BM_PassString_Short | 4.76        | SSO kicks in, no heap alloc  |
| BM_PassView_Short   | **0.32**    | same cost as long — O(1)     |

**Speedup: ~160x (long string), ~15x (short/SSO string)**

### 2. Substring Extraction

| Benchmark       | Median (ns) | Notes                          |
|-----------------|-------------|--------------------------------|
| BM_SubstrString | 20.8        | allocates + copies 100 bytes   |
| BM_SubstrView   | **0.32**    | adjusts pointer and length     |

**Speedup: ~65x**

### 3. Building 64 Substrings

| Benchmark          | Median (ns) | Notes                           |
|--------------------|-------------|---------------------------------|
| BM_VectorOfStrings | 534         | 64 heap allocations             |
| BM_VectorOfViews   | **149**     | 64 pointer copies, zero allocs  |

**Speedup: ~3.6x**

## Conclusion / Takeaway

- Use `std::string_view` for **any read-only string parameter** — the speedup is 15–160x depending on string length.
- `string::substr` is always an allocation. Prefer `string_view::substr` when you only need to inspect the slice.
- Even with SSO (short string optimization), `string_view` is cheaper because SSO still copies bytes into the stack buffer.
- **Lifetime caveat:** `string_view` does not own data. Never store a `string_view` that outlives its source string.

## Further Reading

- [string_view — cppreference](https://en.cppreference.com/w/cpp/string/basic_string_view)
- [CppCon 2018: Marshall Clow — string_view](https://www.youtube.com/watch?v=H9gAaNRoon4)
- [Abseil tip #1: string_view](https://abseil.io/tips/1)

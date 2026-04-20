#include <benchmark/benchmark.h>
#include <algorithm>
#include <unordered_map>
#include <vector>

// Sizes to sweep: small enough to stay in L1/L2 cache
static const std::vector<int64_t> kSizes = {4, 8, 16, 32, 64, 128};

static std::vector<int> make_sorted(int n) {
    std::vector<int> v(n);
    for (int i = 0; i < n; ++i) v[i] = i * 2;  // even numbers as keys
    return v;
}

static std::unordered_map<int, int> make_map(int n) {
    std::unordered_map<int, int> m;
    m.reserve(n);
    for (int i = 0; i < n; ++i) m[i * 2] = i;
    return m;
}

// Lookup a key that exists (hit) and one that doesn't (miss), alternating.
// Using a fixed pattern so the compiler can't hoist the lookup out of the loop.
static int query_key(int iter, int n) {
    // mix hits and misses: even index → hit (key in range), odd → miss
    return (iter % 2 == 0) ? ((iter / 2) % n) * 2 : -1;
}

static void BM_LinearSearch(benchmark::State& state) {
    const int n = state.range(0);
    const auto data = make_sorted(n);
    int iter = 0;
    for (auto _ : state) {
        int key = query_key(iter++, n);
        auto it = std::find(data.begin(), data.end(), key);
        benchmark::DoNotOptimize(it);
    }
}

static void BM_BinarySearch(benchmark::State& state) {
    const int n = state.range(0);
    const auto data = make_sorted(n);
    int iter = 0;
    for (auto _ : state) {
        int key = query_key(iter++, n);
        bool found = std::binary_search(data.begin(), data.end(), key);
        benchmark::DoNotOptimize(found);
    }
}

static void BM_HashMap(benchmark::State& state) {
    const int n = state.range(0);
    const auto map = make_map(n);
    int iter = 0;
    for (auto _ : state) {
        int key = query_key(iter++, n);
        auto it = map.find(key);
        benchmark::DoNotOptimize(it);
    }
}

// Register each benchmark across all sizes
BENCHMARK(BM_LinearSearch)->ArgsProduct({kSizes});
BENCHMARK(BM_BinarySearch)->ArgsProduct({kSizes});
BENCHMARK(BM_HashMap)->ArgsProduct({kSizes});

BENCHMARK_MAIN();

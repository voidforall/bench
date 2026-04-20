#include <benchmark/benchmark.h>
#include <algorithm>
#include <numeric>
#include <random>
#include <vector>

static constexpr int kSize = 1 << 16;  // 64K elements

static std::vector<int> make_data(bool sorted) {
    std::vector<int> v(kSize);
    std::iota(v.begin(), v.end(), 0);
    if (!sorted) {
        std::mt19937 rng(42);
        std::shuffle(v.begin(), v.end(), rng);
    }
    return v;
}

// Conditional sum: if value > threshold, add it
static int64_t sum_conditional(const std::vector<int>& v) {
    const int threshold = kSize / 2;
    int64_t s = 0;
    for (int x : v) {
        if (x > threshold) s += x;
    }
    return s;
}

// Branchless equivalent using subtraction trick
static int64_t sum_branchless(const std::vector<int>& v) {
    const int threshold = kSize / 2;
    int64_t s = 0;
    for (int x : v) {
        // (x > threshold) evaluates to 0 or 1 without a branch
        s += static_cast<int64_t>(x) * (x > threshold);
    }
    return s;
}

static void BM_UnsortedSum(benchmark::State& state) {
    const auto data = make_data(false);
    for (auto _ : state) {
        benchmark::DoNotOptimize(sum_conditional(data));
    }
    state.SetItemsProcessed(state.iterations() * kSize);
}

static void BM_SortedSum(benchmark::State& state) {
    const auto data = make_data(true);
    for (auto _ : state) {
        benchmark::DoNotOptimize(sum_conditional(data));
    }
    state.SetItemsProcessed(state.iterations() * kSize);
}

static void BM_BranchlessSum(benchmark::State& state) {
    const auto data = make_data(false);  // order doesn't matter for branchless
    for (auto _ : state) {
        benchmark::DoNotOptimize(sum_branchless(data));
    }
    state.SetItemsProcessed(state.iterations() * kSize);
}

BENCHMARK(BM_UnsortedSum);
BENCHMARK(BM_SortedSum);
BENCHMARK(BM_BranchlessSum);

BENCHMARK_MAIN();

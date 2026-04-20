#include <benchmark/benchmark.h>
#include <atomic>
#include <thread>
#include <vector>

static constexpr int kThreads = 4;
static constexpr int kIters = 10'000'000;

// Packed: all counters share the same cache line(s) → false sharing
struct PackedCounters {
    std::atomic<int64_t> val[kThreads];
};

// Padded: each counter occupies its own 64-byte cache line → no false sharing
struct alignas(64) PaddedCounter {
    std::atomic<int64_t> val;
    char pad[64 - sizeof(std::atomic<int64_t>)];
};

static void increment_packed(PackedCounters& c, int idx) {
    for (int i = 0; i < kIters; ++i)
        c.val[idx].fetch_add(1, std::memory_order_relaxed);
}

static void increment_padded(PaddedCounter* c, int idx) {
    for (int i = 0; i < kIters; ++i)
        c[idx].val.fetch_add(1, std::memory_order_relaxed);
}

static void BM_FalseSharing(benchmark::State& state) {
    for (auto _ : state) {
        PackedCounters counters{};
        std::vector<std::thread> threads;
        threads.reserve(kThreads);
        for (int t = 0; t < kThreads; ++t)
            threads.emplace_back(increment_packed, std::ref(counters), t);
        for (auto& th : threads) th.join();
        benchmark::DoNotOptimize(counters);
    }
}

static void BM_NoFalseSharing(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<PaddedCounter> counters(kThreads);
        std::vector<std::thread> threads;
        threads.reserve(kThreads);
        for (int t = 0; t < kThreads; ++t)
            threads.emplace_back(increment_padded, counters.data(), t);
        for (auto& th : threads) th.join();
        benchmark::DoNotOptimize(counters);
    }
}

BENCHMARK(BM_FalseSharing)->Unit(benchmark::kMillisecond)->Iterations(10);
BENCHMARK(BM_NoFalseSharing)->Unit(benchmark::kMillisecond)->Iterations(10);

BENCHMARK_MAIN();

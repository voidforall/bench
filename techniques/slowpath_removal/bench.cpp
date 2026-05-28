#include <benchmark/benchmark.h>
#include <cstdint>
#include <random>
#include <vector>

static constexpr int kOrders = 1 << 14;  // 16 K orders

struct Order {
    int64_t value;
    uint8_t err_a, err_b, err_c;  // 0 = ok, 1 = error
};

static const std::vector<Order>& get_orders() {
    static auto v = [] {
        std::mt19937 rng(42);
        std::bernoulli_distribution err(0.01);  // 1% error rate per condition
        std::vector<Order> data(kOrders);
        for (auto& o : data) {
            o.value = static_cast<int64_t>(rng() & 0xFFF) + 1;
            o.err_a = err(rng) ? 1u : 0u;
            o.err_b = err(rng) ? 1u : 0u;
            o.err_c = err(rng) ? 1u : 0u;
        }
        return data;
    }();
    return v;
}

static volatile int64_t g_sink = 0;

// Inlined handler — body duplicated at every call site, bloating the hot loop's
// instruction footprint even though it fires only ~1% of the time.
[[gnu::always_inline]] static inline void handle_inline(uint64_t flags) {
    g_sink += static_cast<int64_t>(flags);
    benchmark::ClobberMemory();
}

// Out-of-line handler — single copy, called far from the hot loop.
[[gnu::noinline]] static void handle_noinline(uint64_t flags) {
    g_sink += static_cast<int64_t>(flags);
    benchmark::ClobberMemory();
}

// ---- Variant 1: chained if/else, handlers inlined (avoid this) ----
// Hot path is the final else; three handler bodies live inside the loop.
static int64_t process_chained_inline(const std::vector<Order>& v) {
    int64_t sum = 0;
    for (const auto& o : v) {
        if (o.err_a)       handle_inline(1);
        else if (o.err_b)  handle_inline(2);
        else if (o.err_c)  handle_inline(4);
        else               sum += o.value;
    }
    return sum;
}

// ---- Variant 2: chained if/else, handlers out-of-line (better) ----
static int64_t process_chained_noinline(const std::vector<Order>& v) {
    int64_t sum = 0;
    for (const auto& o : v) {
        if (o.err_a)       handle_noinline(1);
        else if (o.err_b)  handle_noinline(2);
        else if (o.err_c)  handle_noinline(4);
        else               sum += o.value;
    }
    return sum;
}

// ---- Variant 3: accumulate flags, single out-of-line handler (aim for this) ----
// Single branch in the hot path; OR-reduce keeps check code branchless.
static int64_t process_flags(const std::vector<Order>& v) {
    int64_t sum = 0;
    for (const auto& o : v) {
        const uint64_t flags = static_cast<uint64_t>(o.err_a)
                             | (static_cast<uint64_t>(o.err_b) << 1)
                             | (static_cast<uint64_t>(o.err_c) << 2);
        if (!flags) sum += o.value;
        else        handle_noinline(flags);
    }
    return sum;
}

static void BM_ChainedInlineHandlers(benchmark::State& state) {
    const auto& v = get_orders();
    for (auto _ : state)
        benchmark::DoNotOptimize(process_chained_inline(v));
    state.SetItemsProcessed(state.iterations() * kOrders);
}

static void BM_ChainedNoinlineHandlers(benchmark::State& state) {
    const auto& v = get_orders();
    for (auto _ : state)
        benchmark::DoNotOptimize(process_chained_noinline(v));
    state.SetItemsProcessed(state.iterations() * kOrders);
}

static void BM_FlagsNoinlineHandler(benchmark::State& state) {
    const auto& v = get_orders();
    for (auto _ : state)
        benchmark::DoNotOptimize(process_flags(v));
    state.SetItemsProcessed(state.iterations() * kOrders);
}

BENCHMARK(BM_ChainedInlineHandlers);
BENCHMARK(BM_ChainedNoinlineHandlers);
BENCHMARK(BM_FlagsNoinlineHandler);

BENCHMARK_MAIN();

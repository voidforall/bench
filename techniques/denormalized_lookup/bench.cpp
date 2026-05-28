// Denormalized vs Normalized data layout for hot-path lookups.
//
// Normalized:   Instrument{price, marketId} ─► Markets[marketId] ─► quantityMultiplier
//               (textbook design: each fact stored exactly once, joined at read time)
//
// Denormalized: Instrument{price, quantityMultiplier, marketId}
//               (quantityMultiplier copied into every Instrument; one cacheline = all fields)
//
// Same arithmetic; the only difference is whether computing notional needs one or two loads
// from independent tables. Sweeping N across cache levels shows when the join hurts.

#include <benchmark/benchmark.h>
#include <cstdint>
#include <random>
#include <vector>

// ── Layouts ───────────────────────────────────────────────────────────────
// Real instrument/market rows in production trading systems carry many more
// fields than what's shown in the talk. Padding to 64 B reflects that and
// makes each lookup touch exactly one cacheline.

struct alignas(64) MarketRow {
    int32_t id;
    char    shortName[4];
    int16_t quantityMultiplier;
    char    padding[54];
};
static_assert(sizeof(MarketRow) == 64, "Market row should be one cacheline");

struct alignas(64) InstrumentNorm {
    float   price;
    int32_t marketId;
    char    padding[56];
};
static_assert(sizeof(InstrumentNorm) == 64, "Instrument (normalized) should be one cacheline");

struct alignas(64) InstrumentDenorm {
    float   price;
    int16_t quantityMultiplier;  // duplicated from the Market table
    // 2 bytes implicit padding here so marketId is 4-byte aligned
    int32_t marketId;            // kept so the struct stays the same size as the normalized one
    char    padding[52];
};
static_assert(sizeof(InstrumentDenorm) == 64, "Instrument (denormalized) should be one cacheline");

// ── Data setup ────────────────────────────────────────────────────────────
// N instruments, N markets. Each instrument is assigned a uniformly random
// marketId so the access pattern through the Markets table is also random —
// the worst case for the normalized layout's cache behavior.

static std::vector<MarketRow> make_markets(int n) {
    std::vector<MarketRow> v(n);
    std::mt19937 rng(7);
    std::uniform_int_distribution<int> mult(10, 1000);
    for (int i = 0; i < n; ++i) {
        v[i].id = i;
        v[i].shortName[0] = 'M';
        v[i].quantityMultiplier = static_cast<int16_t>(mult(rng));
    }
    return v;
}

static std::vector<InstrumentNorm> make_instruments_norm(int n) {
    std::vector<InstrumentNorm> v(n);
    std::mt19937 rng(11);
    std::uniform_int_distribution<int> mkt(0, n - 1);
    for (int i = 0; i < n; ++i) {
        v[i].price = 100.0f + (i % 50);
        v[i].marketId = mkt(rng);
    }
    return v;
}

static std::vector<InstrumentDenorm> make_instruments_denorm(int n, const std::vector<MarketRow>& markets) {
    std::vector<InstrumentDenorm> v(n);
    std::mt19937 rng(11);  // SAME seed → same marketId assignments as normalized case
    std::uniform_int_distribution<int> mkt(0, n - 1);
    for (int i = 0; i < n; ++i) {
        v[i].price = 100.0f + (i % 50);
        v[i].marketId = mkt(rng);
        v[i].quantityMultiplier = markets[v[i].marketId].quantityMultiplier;
    }
    return v;
}

// Pre-generated random access indices into the instrument table — simulating
// an inbound order stream that touches instruments in unpredictable order.
static constexpr int kOpsPerIter = 4096;
static std::vector<int> make_indices(int n) {
    std::vector<int> v(kOpsPerIter);
    std::mt19937 rng(23);
    std::uniform_int_distribution<int> dist(0, n - 1);
    for (auto& i : v) i = dist(rng);
    return v;
}

// ── Benchmarks ────────────────────────────────────────────────────────────
// Body of both loops computes the same thing: notional = price * qty * multiplier.
// Normalized version pays an extra load + cache miss to fetch the multiplier.

static void BM_Normalized(benchmark::State& state) {
    const int n = static_cast<int>(state.range(0));
    const auto markets     = make_markets(n);
    const auto instruments = make_instruments_norm(n);
    const auto indices     = make_indices(n);
    const int qty = 17;

    for (auto _ : state) {
        double notional = 0;
        for (int idx : indices) {
            const auto& ins = instruments[idx];
            const auto& mkt = markets[ins.marketId];     // pointer chase
            notional += double(ins.price) * qty * mkt.quantityMultiplier;
        }
        benchmark::DoNotOptimize(notional);
    }
    state.SetItemsProcessed(state.iterations() * kOpsPerIter);
}

static void BM_Denormalized(benchmark::State& state) {
    const int n = static_cast<int>(state.range(0));
    const auto markets     = make_markets(n);
    const auto instruments = make_instruments_denorm(n, markets);
    const auto indices     = make_indices(n);
    const int qty = 17;

    for (auto _ : state) {
        double notional = 0;
        for (int idx : indices) {
            const auto& ins = instruments[idx];
            notional += double(ins.price) * qty * ins.quantityMultiplier;
        }
        benchmark::DoNotOptimize(notional);
    }
    state.SetItemsProcessed(state.iterations() * kOpsPerIter);
}

// Sizes chosen to walk through cache levels on an Apple M1 Pro
// (64 KiB L1d, 4 MiB L2). Each row is 64 B, so working-set bytes
// for the normalized case is 2*N*64 (both tables touched):
//   N=128      → 16 KiB     (fits in L1)
//   N=1024     → 128 KiB    (spills to L2)
//   N=16384    → 2 MiB      (fits in L2)
//   N=262144   → 32 MiB     (spills to DRAM)
static const std::vector<int64_t> kSizes = {128, 1024, 16384, 262144};

BENCHMARK(BM_Normalized)->ArgsProduct({kSizes});
BENCHMARK(BM_Denormalized)->ArgsProduct({kSizes});

BENCHMARK_MAIN();

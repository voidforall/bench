// Cache priming: keep i-cache and d-cache hot by running dummy traffic through
// the real pipeline between sparse real events.
//
// The pattern from the talk:
//   Market data decoder ─► Strategy ─► Execution engine ─► (suppress send)
// runs frequently with dummies; the rare "real" message goes the same path
// except the final send actually fires. Both share the same code + data, so
// the dummies keep the L1/L2/branch-predictor primed for the real one.
//
// What makes this hard to bench fairly:
//   * "cold" needs to be reproducible — we evict caches with a 16 MiB scan
//     before each measured iteration.
//   * "warmed by dummy" must time ONLY the real event — the dummy run is
//     overhead we accept system-side. We use UseManualTime + steady_clock so
//     the dummy invocation is outside the timed region.
//   * Measurement granularity: a single real call is ~tens-of-ns, which is
//     below steady_clock resolution on macOS (~42 ns mach-tick). We time a
//     batch of 512 messages per measurement so the timed region is a few
//     microseconds — well above the clock floor — without losing the
//     cold-vs-warm contrast (each batch still touches each cacheline once).
//   * `noinline` on pipeline_step is critical: it makes the dummy and real
//     paths share the SAME instruction-cache lines. Otherwise the compiler
//     would inline + constant-fold the `dummy` flag into two distinct loops
//     and the i-cache warming argument falls apart.

#include <benchmark/benchmark.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <random>
#include <vector>

// ─── Pipeline data tables ──────────────────────────────────────────────
// Sized so the union of touched cachelines under one batch fits in L1d
// (~64 KiB), but the table backing storage exceeds L1 — meaning after a
// cache flush, the first batch pays full reload cost.

struct alignas(64) DecoderRow { int32_t factor; char pad[60]; };
struct alignas(64) Instrument { int32_t marketId; float price; int32_t qty_mult; char pad[52]; };
struct alignas(64) RiskRow    { int64_t limit;   char pad[56]; };

constexpr int kDecoders    = 32;     // 2 KiB
constexpr int kInstruments = 4096;   // 256 KiB — exceeds L1d, fits L2
constexpr int kRiskRows    = 256;    // 16 KiB
constexpr int kBatch       = 512;    // messages per measured batch

static auto make_decoders() {
    std::array<DecoderRow, kDecoders> v{};
    for (int i = 0; i < kDecoders; ++i) v[i].factor = i + 1;
    return v;
}

static auto make_instruments() {
    std::vector<Instrument> v(kInstruments);
    for (int i = 0; i < kInstruments; ++i) {
        v[i].marketId = i % kRiskRows;
        v[i].price    = 100.0f + (i % 50);
        v[i].qty_mult = 1 + (i % 10);
    }
    return v;
}

static auto make_risk() {
    std::vector<RiskRow> v(kRiskRows);
    for (int i = 0; i < kRiskRows; ++i) v[i].limit = 1'000'000LL + i * 100;
    return v;
}

struct Message { int8_t type; int32_t instId; int32_t qty; };

// Same batch of messages used for both dummy and real runs — simulates the
// "perfect warming" upper bound. A real system's dummies won't cover every
// real instrument, so production wins will be somewhat smaller than this.
static auto make_messages() {
    std::array<Message, kBatch> m{};
    std::mt19937 rng(99);
    std::uniform_int_distribution<int> inst(0, kInstruments - 1);
    for (int i = 0; i < kBatch; ++i) {
        m[i] = {static_cast<int8_t>(i % kDecoders), inst(rng), 100};
    }
    return m;
}

// ─── The pipeline ──────────────────────────────────────────────────────

__attribute__((noinline))
static int64_t pipeline_step(
    const Message& msg,
    const std::array<DecoderRow, kDecoders>& decoders,
    const std::vector<Instrument>& instruments,
    const std::vector<RiskRow>& risk,
    bool dummy)
{
    const auto& dec = decoders[msg.type & (kDecoders - 1)];
    int decoded = dec.factor * msg.qty;

    const auto& ins = instruments[msg.instId & (kInstruments - 1)];
    int64_t notional = static_cast<int64_t>(ins.price) * decoded * ins.qty_mult;

    const auto& rsk = risk[ins.marketId & (kRiskRows - 1)];
    if (notional > rsk.limit) notional = -1;

    if (dummy) {
        // production: this is the only branch that differs from real — a
        // single check that suppresses the actual exchange send.
        benchmark::DoNotOptimize(notional);
        return 0;
    }
    return notional;
}

__attribute__((noinline))
static int64_t run_batch(
    const std::array<Message, kBatch>& msgs,
    const std::array<DecoderRow, kDecoders>& decoders,
    const std::vector<Instrument>& instruments,
    const std::vector<RiskRow>& risk,
    bool dummy)
{
    int64_t total = 0;
    for (const auto& m : msgs) {
        total += pipeline_step(m, decoders, instruments, risk, dummy);
    }
    return total;
}

// ─── Cache polluter ────────────────────────────────────────────────────
// 16 MiB sequential scan. Evicts L1d (64 KiB) and L2 (4 MiB) thoroughly.
// We *don't* try to evict i-cache here — modern i-caches are large enough
// that the pollute scan itself doesn't displace pipeline code. The cold
// effect we measure is dominated by d-cache, which is what the talk's
// dummies primarily aim to keep warm anyway.

constexpr size_t kPolluteBytes = 16 * 1024 * 1024;

struct Polluter {
    std::vector<char> data;
    Polluter() : data(kPolluteBytes, 1) {}
    void flush() {
        char acc = 0;
        for (size_t i = 0; i < data.size(); i += 64) acc ^= data[i];
        benchmark::DoNotOptimize(acc);
    }
};

// ─── Benchmarks ────────────────────────────────────────────────────────

using Clock = std::chrono::steady_clock;

static void BM_Cold(benchmark::State& state) {
    auto decoders    = make_decoders();
    auto instruments = make_instruments();
    auto risk        = make_risk();
    auto msgs        = make_messages();
    Polluter pol;

    for (auto _ : state) {
        pol.flush();                                            // simulate idle / background load
        auto start = Clock::now();
        auto r = run_batch(msgs, decoders, instruments, risk, /*dummy=*/false);
        auto end = Clock::now();
        benchmark::DoNotOptimize(r);
        state.SetIterationTime(std::chrono::duration<double>(end - start).count());
    }
}

static void BM_WarmedByDummy(benchmark::State& state) {
    auto decoders    = make_decoders();
    auto instruments = make_instruments();
    auto risk        = make_risk();
    auto msgs        = make_messages();
    Polluter pol;

    for (auto _ : state) {
        pol.flush();
        auto d = run_batch(msgs, decoders, instruments, risk, /*dummy=*/true);  // primes caches (untimed)
        benchmark::DoNotOptimize(d);
        auto start = Clock::now();
        auto r = run_batch(msgs, decoders, instruments, risk, /*dummy=*/false);
        auto end = Clock::now();
        benchmark::DoNotOptimize(r);
        state.SetIterationTime(std::chrono::duration<double>(end - start).count());
    }
}

static void BM_HotBaseline(benchmark::State& state) {
    auto decoders    = make_decoders();
    auto instruments = make_instruments();
    auto risk        = make_risk();
    auto msgs        = make_messages();

    // pre-warm before the timed loop
    for (int i = 0; i < 4; ++i) {
        auto r = run_batch(msgs, decoders, instruments, risk, false);
        benchmark::DoNotOptimize(r);
    }

    for (auto _ : state) {
        auto start = Clock::now();
        auto r = run_batch(msgs, decoders, instruments, risk, /*dummy=*/false);
        auto end = Clock::now();
        benchmark::DoNotOptimize(r);
        state.SetIterationTime(std::chrono::duration<double>(end - start).count());
    }
}

BENCHMARK(BM_Cold)->UseManualTime();
BENCHMARK(BM_WarmedByDummy)->UseManualTime();
BENCHMARK(BM_HotBaseline)->UseManualTime();

BENCHMARK_MAIN();

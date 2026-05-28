#include <benchmark/benchmark.h>
#include <cstdint>
#include <random>
#include <vector>

static constexpr int kOrders = 1 << 14;  // 16 K

enum class Side : uint8_t { Buy, Sell };

struct Order {
    float fair;
    float credit;
    float spread;
    float risk;
    Side  side;
};

static std::vector<Order> make_orders(bool mixed) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> fdist(95.0f,  105.0f);
    std::uniform_real_distribution<float> cdist(0.05f,  0.50f);
    std::uniform_real_distribution<float> spdist(0.01f, 0.10f);
    std::uniform_real_distribution<float> rdist(0.001f, 0.01f);
    std::bernoulli_distribution           bdist(0.5);
    std::vector<Order> v(kOrders);
    for (auto& o : v) {
        o.fair   = fdist(rng);
        o.credit = cdist(rng);
        o.spread = spdist(rng);
        o.risk   = rdist(rng);
        o.side   = mixed ? (bdist(rng) ? Side::Buy : Side::Sell) : Side::Buy;
    }
    return v;
}

static const std::vector<Order>& orders_homogeneous() {
    static auto v = make_orders(false);
    return v;
}
static const std::vector<Order>& orders_mixed() {
    static auto v = make_orders(true);
    return v;
}

// ── Branching helpers ──────────────────────────────────────────────────────────
// noinline: forces real function-call boundaries, preventing the compiler from
// fusing both variants into a single fcsel.
// __builtin_expect: marks Buy as the likely path, nudging the compiler to emit
// a conditional branch (cbz/cbnz) rather than eager-evaluate-both + fcsel.
// Each path has 3 distinct FP ops with different constants so the compiler
// cannot easily represent both as fcsel without inflating instruction count.

[[gnu::noinline]] static float calc_price(Side s, float fair, float credit) {
    if (__builtin_expect(s == Side::Buy, 1)) {
        float base = fair - credit;
        return base * (1.0f - credit * 0.001f);
    }
    float base = fair + credit;
    return base * (1.0f + credit * 0.0015f);
}

[[gnu::noinline]] static float apply_spread(Side s, float price, float spread) {
    if (__builtin_expect(s == Side::Buy, 1)) {
        return price + spread * (1.0f - price * 0.0005f);
    }
    return price - spread * (1.0f + price * 0.0005f);
}

[[gnu::noinline]] static float size_adjusted(Side s, float price, float risk) {
    if (__builtin_expect(s == Side::Buy, 1)) {
        return price * (1.0f - risk) * (1.0f - risk * 0.5f);
    }
    return price * (1.0f + risk) * (1.0f + risk * 0.5f);
}

static float run_branch(const Order& o) {
    float p = calc_price(o.side, o.fair, o.credit);
    p = apply_spread(o.side, p, o.spread);
    return size_adjusted(o.side, p, o.risk);
}

// ── Template helpers ───────────────────────────────────────────────────────────
// noinline: same call-boundary discipline as the branching version.
// if constexpr: the dead side is removed entirely at compile time — each
// instantiation is a single straight-line arithmetic function, zero branches.

template <Side S>
[[gnu::noinline]] static float calc_price_t(float fair, float credit) {
    if constexpr (S == Side::Buy) {
        float base = fair - credit;
        return base * (1.0f - credit * 0.001f);
    } else {
        float base = fair + credit;
        return base * (1.0f + credit * 0.0015f);
    }
}

template <Side S>
[[gnu::noinline]] static float apply_spread_t(float price, float spread) {
    if constexpr (S == Side::Buy) {
        return price + spread * (1.0f - price * 0.0005f);
    } else {
        return price - spread * (1.0f + price * 0.0005f);
    }
}

template <Side S>
[[gnu::noinline]] static float size_adjusted_t(float price, float risk) {
    if constexpr (S == Side::Buy) {
        return price * (1.0f - risk) * (1.0f - risk * 0.5f);
    } else {
        return price * (1.0f + risk) * (1.0f + risk * 0.5f);
    }
}

template <Side S>
static float run_tmpl(const Order& o) {
    float p = calc_price_t<S>(o.fair, o.credit);
    p = apply_spread_t<S>(p, o.spread);
    return size_adjusted_t<S>(p, o.risk);
}

// ── Process loops ─────────────────────────────────────────────────────────────

static float process_branch(const std::vector<Order>& v) {
    float sum = 0.f;
    for (const auto& o : v)
        sum += run_branch(o);
    return sum;
}

// Homogeneous batch: Side known at compile time, no dispatch branch at all
static float process_tmpl_buy(const std::vector<Order>& v) {
    float sum = 0.f;
    for (const auto& o : v)
        sum += run_tmpl<Side::Buy>(o);
    return sum;
}

// Mixed batch: one dispatch branch per order, but all three helpers are branch-free
static float process_tmpl_mixed(const std::vector<Order>& v) {
    float sum = 0.f;
    for (const auto& o : v) {
        if (o.side == Side::Buy) sum += run_tmpl<Side::Buy>(o);
        else                     sum += run_tmpl<Side::Sell>(o);
    }
    return sum;
}

// ── Benchmarks ────────────────────────────────────────────────────────────────

static void BM_Branch_Homogeneous(benchmark::State& state) {
    const auto& v = orders_homogeneous();
    for (auto _ : state)
        benchmark::DoNotOptimize(process_branch(v));
    state.SetItemsProcessed(state.iterations() * kOrders);
}

static void BM_Template_Homogeneous(benchmark::State& state) {
    const auto& v = orders_homogeneous();
    for (auto _ : state)
        benchmark::DoNotOptimize(process_tmpl_buy(v));
    state.SetItemsProcessed(state.iterations() * kOrders);
}

static void BM_Branch_Mixed(benchmark::State& state) {
    const auto& v = orders_mixed();
    for (auto _ : state)
        benchmark::DoNotOptimize(process_branch(v));
    state.SetItemsProcessed(state.iterations() * kOrders);
}

static void BM_Template_Mixed(benchmark::State& state) {
    const auto& v = orders_mixed();
    for (auto _ : state)
        benchmark::DoNotOptimize(process_tmpl_mixed(v));
    state.SetItemsProcessed(state.iterations() * kOrders);
}

BENCHMARK(BM_Branch_Homogeneous);
BENCHMARK(BM_Template_Homogeneous);
BENCHMARK(BM_Branch_Mixed);
BENCHMARK(BM_Template_Mixed);

BENCHMARK_MAIN();

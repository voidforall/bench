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
    Side  side;
};

static std::vector<Order> make_orders(bool mixed) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> fdist(95.0f, 105.0f);
    std::uniform_real_distribution<float> cdist(0.05f, 0.50f);
    std::uniform_real_distribution<float> spdist(0.01f, 0.10f);
    std::bernoulli_distribution           bdist(0.5);
    std::vector<Order> v(kOrders);
    for (auto& o : v) {
        o.fair   = fdist(rng);
        o.credit = cdist(rng);
        o.spread = spdist(rng);
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

// ── Branching: Side is a runtime value; each helper branches on it ────────────

static float calc_price(Side s, float fair, float credit) {
    return s == Side::Buy ? fair - credit : fair + credit;
}
static float apply_spread(Side s, float price, float spread) {
    return s == Side::Buy ? price + spread : price - spread;
}
static float position_sign(Side s, float qty) {
    return s == Side::Buy ? qty : -qty;
}

static float run_branch(const Order& o) {
    float price = calc_price(o.side, o.fair, o.credit);
    price       = apply_spread(o.side, price, o.spread);
    return position_sign(o.side, price);
}

// ── Templates: Side is a compile-time constant; if constexpr removed at ──────
//              compile time, leaving straight-line arithmetic ──────────────────

template <Side S>
static float calc_price_t(float fair, float credit) {
    if constexpr (S == Side::Buy) return fair - credit;
    else                          return fair + credit;
}
template <Side S>
static float apply_spread_t(float price, float spread) {
    if constexpr (S == Side::Buy) return price + spread;
    else                          return price - spread;
}
template <Side S>
static float position_sign_t(float qty) {
    if constexpr (S == Side::Buy) return qty;
    else                          return -qty;
}

template <Side S>
static float run_tmpl(const Order& o) {
    float price = calc_price_t<S>(o.fair, o.credit);
    price       = apply_spread_t<S>(price, o.spread);
    return position_sign_t<S>(price);
}

// ── Process loops ─────────────────────────────────────────────────────────────

static float process_branch(const std::vector<Order>& v) {
    float sum = 0;
    for (const auto& o : v)
        sum += run_branch(o);
    return sum;
}

// Side known at compile time — zero dispatch overhead
static float process_tmpl_buy(const std::vector<Order>& v) {
    float sum = 0;
    for (const auto& o : v)
        sum += run_tmpl<Side::Buy>(o);
    return sum;
}

// Mixed: one dispatch branch, but all helpers are branch-free inside
static float process_tmpl_mixed(const std::vector<Order>& v) {
    float sum = 0;
    for (const auto& o : v) {
        if (o.side == Side::Buy) sum += run_tmpl<Side::Buy>(o);
        else                     sum += run_tmpl<Side::Sell>(o);
    }
    return sum;
}

// ── Benchmarks ────────────────────────────────────────────────────────────────

// Homogeneous stream: branch predictor learns the pattern for the branching
// version, but template version still wins by having zero conditional ops.
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

// Mixed 50/50 stream: branch predictor is maximally stressed in the branching
// version (3 unpredictable branches per order). Template version has one
// dispatch branch plus zero branches inside the helpers.
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

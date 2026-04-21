#include <benchmark/benchmark.h>
#include <string>
#include <string_view>
#include <vector>

// ── shared data ──────────────────────────────────────────────────────────────

static const std::string kLong  = std::string(256, 'x');  // heap-allocated
static const std::string kShort = "hello";                // may use SSO

// ── 1. Argument passing: by-value copy vs string_view ────────────────────────
//    Models a function that only reads the string (no mutation needed).

static size_t consume_string(std::string s) {
    return s.size();
}
static size_t consume_view(std::string_view sv) {
    return sv.size();
}

static void BM_PassString_Long(benchmark::State& state) {
    for (auto _ : state)
        benchmark::DoNotOptimize(consume_string(kLong));
}
static void BM_PassView_Long(benchmark::State& state) {
    for (auto _ : state)
        benchmark::DoNotOptimize(consume_view(kLong));
}
static void BM_PassString_Short(benchmark::State& state) {
    for (auto _ : state)
        benchmark::DoNotOptimize(consume_string(kShort));
}
static void BM_PassView_Short(benchmark::State& state) {
    for (auto _ : state)
        benchmark::DoNotOptimize(consume_view(kShort));
}

// ── 2. Substring: string::substr (allocates) vs string_view::substr (zero-copy)

static void BM_SubstrString(benchmark::State& state) {
    for (auto _ : state) {
        std::string sub = kLong.substr(10, 100);
        benchmark::DoNotOptimize(sub);
    }
}
static void BM_SubstrView(benchmark::State& state) {
    std::string_view sv = kLong;
    for (auto _ : state) {
        std::string_view sub = sv.substr(10, 100);
        benchmark::DoNotOptimize(sub);
    }
}

// ── 3. Building a vector of substrings (allocation pressure) ─────────────────

static constexpr int kSlices = 64;

static void BM_VectorOfStrings(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<std::string> v;
        v.reserve(kSlices);
        for (int i = 0; i < kSlices; ++i)
            v.push_back(kLong.substr(i * 2, 8));
        benchmark::DoNotOptimize(v);
    }
}
static void BM_VectorOfViews(benchmark::State& state) {
    std::string_view sv = kLong;
    for (auto _ : state) {
        std::vector<std::string_view> v;
        v.reserve(kSlices);
        for (int i = 0; i < kSlices; ++i)
            v.push_back(sv.substr(i * 2, 8));
        benchmark::DoNotOptimize(v);
    }
}

// ─────────────────────────────────────────────────────────────────────────────

BENCHMARK(BM_PassString_Long);
BENCHMARK(BM_PassView_Long);
BENCHMARK(BM_PassString_Short);
BENCHMARK(BM_PassView_Short);
BENCHMARK(BM_SubstrString);
BENCHMARK(BM_SubstrView);
BENCHMARK(BM_VectorOfStrings);
BENCHMARK(BM_VectorOfViews);

BENCHMARK_MAIN();

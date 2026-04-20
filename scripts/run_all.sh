#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
RESULTS="$ROOT/results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

cmake -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O2 -march=native" -S "$ROOT"
cmake --build "$BUILD" -j"$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)"

mkdir -p "$RESULTS"

for bench in "$BUILD"/techniques/*/bench_*; do
    name=$(basename "$bench")
    out="$RESULTS/${name}_${TIMESTAMP}.txt"
    echo "=== Running $name ===" | tee "$out"
    "$bench" --benchmark_repetitions=5 --benchmark_report_aggregates_only=true 2>&1 | tee -a "$out"
    echo ""
done

echo "Results saved to $RESULTS/"

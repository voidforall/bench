add_executable(bench_cache_priming bench.cpp)
target_link_libraries(bench_cache_priming benchmark::benchmark)
target_compile_options(bench_cache_priming PRIVATE -O2 -march=native)

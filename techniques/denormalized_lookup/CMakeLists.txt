add_executable(bench_denormalized_lookup bench.cpp)
target_link_libraries(bench_denormalized_lookup benchmark::benchmark)
target_compile_options(bench_denormalized_lookup PRIVATE -O2 -march=native)

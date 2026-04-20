add_executable(bench_small_data_lookup bench.cpp)
target_link_libraries(bench_small_data_lookup benchmark::benchmark)
target_compile_options(bench_small_data_lookup PRIVATE -O2 -march=native)

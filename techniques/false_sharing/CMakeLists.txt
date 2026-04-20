add_executable(bench_false_sharing bench.cpp)
target_link_libraries(bench_false_sharing benchmark::benchmark)
target_compile_options(bench_false_sharing PRIVATE -O2 -march=native)

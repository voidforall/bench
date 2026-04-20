add_executable(bench_branch_prediction bench.cpp)
target_link_libraries(bench_branch_prediction benchmark::benchmark)
target_compile_options(bench_branch_prediction PRIVATE -O2 -march=native)

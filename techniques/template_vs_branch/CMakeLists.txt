add_executable(bench_template_vs_branch bench.cpp)
target_link_libraries(bench_template_vs_branch benchmark::benchmark)
target_compile_options(bench_template_vs_branch PRIVATE -O2 -march=native)

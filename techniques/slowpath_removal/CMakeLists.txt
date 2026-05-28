add_executable(bench_slowpath_removal bench.cpp)
target_link_libraries(bench_slowpath_removal benchmark::benchmark)
target_compile_options(bench_slowpath_removal PRIVATE -O2 -march=native)

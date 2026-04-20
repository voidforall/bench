# Adding a New Technique

1. Create a subdirectory:
   ```bash
   mkdir techniques/<technique_name>
   ```

2. Add three files:

   **`techniques/<technique_name>/CMakeLists.txt`**
   ```cmake
   add_executable(bench_<technique_name> bench.cpp)
   target_link_libraries(bench_<technique_name> benchmark::benchmark)
   target_compile_options(bench_<technique_name> PRIVATE -O2 -march=native)
   ```

   **`techniques/<technique_name>/bench.cpp`**
   - Include `<benchmark/benchmark.h>`
   - Implement at least two `BM_*` functions: one "naive" and one "optimized"
   - Call `BENCHMARK(BM_...)` for each
   - End with `BENCHMARK_MAIN()`

   **`techniques/<technique_name>/README.md`**
   - Copy the template from any existing technique README
   - Fill in: What It Measures, Why It Matters, How To Run, Results (placeholder), Conclusion, Further Reading

3. The root `techniques/CMakeLists.txt` auto-discovers subdirectories — no manual registration needed.

4. Add a row to the index table in the root `README.md`.

5. Build and verify:
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j --target bench_<technique_name>
   ./build/techniques/<technique_name>/bench_<technique_name> --benchmark_repetitions=5
   ```

6. Run on your machine, fill in the Results table, commit.

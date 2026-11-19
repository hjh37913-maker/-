# Coverage report for implemented code

One GCC 12.2.0 Linux x86-64 Debug `--coverage -O0` build; counters cleared before one complete CTest run on 2026-11-19. The report covers the eight compiled `nightfall` library `.cpp` files, not all project files, platform-specific code, unimplemented features, game compatibility or correctness. Counts are GCC/gcov's instrumented source-line and branch counts (including compiler-generated exception branches); a covered line is not proof that every behavior was verified.

| Source | Executed / instrumented lines | Taken / instrumented branches |
|---|---:|---:|
| CPU | 267 / 267 | 464 / 517 |
| ELF loader | 45 / 54 | 92 / 119 |
| Core machine | 8 / 8 | 6 / 7 |
| Memory | 49 / 49 | 59 / 70 |
| Storage | 28 / 31 | 36 / 55 |
| Input | 27 / 27 | 36 / 48 |
| Software graphics | 42 / 44 | 55 / 63 |
| Logical timer | 30 / 30 | 33 / 38 |

For example, allocation-failure handlers in storage and graphics and a storage short-read failure were not hit. Some branches are exception or defensive paths. No fault-injection, true GPU, physical input, Windows, actual guest, long-duration or fuzz coverage was measured.

## Reproduce on Linux with GCC/gcov
```sh
cmake -S . -B build-coverage -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='--coverage -O0' -DCMAKE_EXE_LINKER_FLAGS='--coverage'
cmake --build build-coverage --parallel 2
# Optional: reset *only generated coverage counters* before each measured run:
find build-coverage -name '*.gcda' -delete
ctest --test-dir build-coverage --output-on-failure
mkdir -p build-coverage/gcov-json
cd build-coverage/gcov-json
for f in ../CMakeFiles/nightfall.dir/src/*/*.gcno; do gcov -j -b "$f"; done
```
The compressed gcov JSON includes library and standard header entries; the table above filters project `.cpp` files. Counters accumulate between runs unless reset. The generated files are ignored and not included in the project archive.

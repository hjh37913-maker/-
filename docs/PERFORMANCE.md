# Performance: first synthetic baseline

Build with `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DNIGHTFALL_BUILD_BENCHMARKS=ON`, then `cmake --build build-release --config Release`. Run `./build-release/cpu_add_benchmark` (multi-config generators may put it under `Release/`). The benchmark executes one million interpreted `ADD RAX, 1` instructions, resetting RIP in the host between steps, and verifies the final RAX. Timing includes loop, reset, decode, guest reads and flags. It does not measure PS5 software, overall CPU throughput or frame rate.

One observed run, 2026-11-19: Linux x86-64 sandbox, reported Intel Xeon Processor @ 2.60 GHz, GCC 12.2.0, CMake Release, 1,000,000 iterations, 21,053,500 ns elapsed, 21.0535 ns/iteration. This single unpinned virtualized run has no warmup, variance study or cross-strategy comparison; it is only a reproducible workload and initial reference, not a performance guarantee. No optimization has been evaluated against it.

## After byte-level access checks
Same environment and workload, one Release run after mapping and per-byte permission checks: 28,242,236 ns total, 28.2422 ns/iteration. The earlier 21.0535 ns/iteration was before these checks. These are single, unpinned runs, not a controlled comparison; the change adds work and metadata, but the numbers alone do not establish a stable regression. Optimize only after repeated measurements and correctness tests.

## After instruction expansion
Same ADD workload and environment, one Release run after adding SUB/CMP and branches: 32,032,796 ns total, 32.0328 ns/iteration. This includes the new instruction dispatch logic. Single unpinned samples cannot establish a reliable regression; repeat controlled measurements before making optimization claims. SUB, CMP and branches have not been benchmarked independently.

## Storage checksum experiment
On the same Linux/GCC 12.2 Release environment, one unpinned run of `storage_crc_benchmark` computed CRC-32 over a synthetic 1 MiB buffer 32 times in 250,535,446 ns (checksum `04d0e435`). Run via the same opt-in benchmark build described above. This measures in-memory checksum work only, not file I/O, cache behavior or PS5 throughput. It is a single sample, not a comparative or optimized result.

## Synthetic input mapping
On the same Linux x86-64 / GCC 12.2 Release environment, one run of `input_mapper_benchmark` performed 1,000,000 alternating synthetic button updates and action queries in 31,995,931 ns; 500,000 active observations were checked. Two devices and two action bindings were configured. This is an unpinned single run; it excludes OS polling, physical devices and guest processing and is not a latency guarantee.

## CPU software graphics
A single unpinned Linux x86-64 GCC 12.2 Release run of `graphics_software_benchmark` executed 10,000 batches, each clearing a 256×256 RGBA8 CPU surface, filling a 128×128 rectangle, then performing an overlapping 128×128 copy: 297,337,526 ns total, 29,733.8 ns per batch. Output pixels were checked. This is a narrow synthetic CPU baseline, not a GPU measurement, frame rate, or game workload.

## Synthetic logical timers
One unpinned Linux x86-64 GCC 12.2 Release run of `runtime_timer_benchmark` scheduled and delivered 320,000 events in 10,000 batches in 14,601,688 ns. This includes map allocations, delivery and checks for a 32-event queue. It excludes wall-clock timing, threads, guest execution and PS5 behavior; it is a single sample, not an optimization claim.

## Register CPU experiment
One unpinned Linux x86-64 GCC 12.2 Release run after adding register forms executed one million interpreted `ADD RAX,RBX` operations with host RIP reset each iteration: 27,178,989 ns total, 27.179 ns/iteration. A separate immediate ADD run measured 32,174,418 ns. These are different workloads, not a controlled comparison or a game performance result.

## CPU data-memory experiment
One unpinned Linux x86-64 GCC 12.2 Release run of `cpu_memory_benchmark` executed one million interpreted MOV store/load pairs, resetting RIP per pair, in 78,987,769 ns (78.9878 ns per pair). This includes guest permission checks and is a single synthetic measurement, not game or PS5 performance.

## Synthetic CPU stack
One unpinned Linux x86-64 GCC 12.2 Release run executed one million interpreted PUSH/POP pairs, resetting RIP each pair: 37,218,778 ns total, 37.2188 ns per pair. It includes guest memory checks and is a single synthetic measurement, not game or PS5 performance.

## Synthetic ELF loading
One unpinned Linux x86-64 GCC 12.2 Release run of `elf_loader_benchmark examples/elf-demo.elf` loaded and executed the original synthetic ELF 5,000 times in 274,485,184 ns (54,897 ns per iteration). Timing includes creating and mapping the synthetic machine and five interpreted instructions; it excludes host file reads. It does not measure PS5 software or game performance.

## CPU imm8 experiment
One unpinned Linux x86-64 GCC 12.2 Release run of `cpu_imm8_benchmark` executed one million interpreted ADD RAX, imm8 operations, resetting RIP each iteration: 37,334,673 ns total, 37.3347 ns per iteration. This synthetic single sample is not a PS5 or game performance result.

## Synthetic TEST/JNZ loop
One unpinned Linux x86-64 GCC 12.2 Release run of `cpu_branch_benchmark` executed one million interpreted TEST/JNZ pairs in 44,721,239 ns (44.7212 ns per pair). This is a narrow software-only workload and a single sample, not game or PS5 performance.

## CPU 32-bit imm8 experiment
One unpinned Linux x86-64 GCC 12.2 Release run of `cpu32_benchmark` executed one million interpreted ADD EAX, imm8 operations with host RIP reset: 27,906,166 ns total, 27.9062 ns/iteration. This single synthetic sample does not represent PS5 or game performance.

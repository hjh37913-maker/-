# Test registry
Run `ctest --test-dir build --output-on-failure` after a Debug build. The first four entries are checks inside `nightfall_tests`; actual result below was observed 2026-11-19 on Linux x86-64, GCC 12.2.0, Debug, Release and ASan/UBSan. TESTED means only these observed checks, not independent verification or full coverage.

| ID | Purpose and procedure | Expected | Actual | Status |
|---|---|---|---|---|
| MEM-001 | Run bounds/read/write checks, including overflow and end boundary | Valid ranges copy; invalid ranges fail without modification | Passed | TESTED |
| CPU-001 | Run MOV/ADD/HLT synthetic program, check budget and result | RAX=42, budget exhaustion then halt | Passed | TESTED |
| CPU-002 | Execute arithmetic boundary examples | Expected selected carry, zero, parity, auxiliary and overflow flags | Passed | TESTED |
| CPU-003 | Execute truncated and invalid instructions, zero budget | Fault/invalid/budget stop; no state mutation on errors | Passed | TESTED |
| MEM-002 | Run `memory_mapping_tests`: mapping, overlap, bounds and counts | Invalid operations leave mapping unchanged; counts match | Passed | TESTED |
| MEM-003 | Run permission, mixed-range write and remapping checks | R/W/X enforced, no partial writes, remapping zeroed | Passed | TESTED |
| CPU-005 | Run execute-permission checks in `memory_mapping_tests` | Fault leaves CPU unchanged; execute-only fetch works | Passed | TESTED |
| CPU-006 | Run `cpu_fault_tests` with every truncation of MOV, ADD, SUB and CMP | Fault and unchanged CPU state, complete instruction succeeds | Passed | TESTED |
| CPU-007 | Run `cpu_branch_tests` for JMP/JZ/JNZ | Correct taken and untaken targets, negative offsets, flags preserved, faults and budget handling | Passed | TESTED |
| CPU-008 | Run synthetic MOV/SUB/CMP/JNZ/HLT loop in `cpu_branch_tests` | Halts after decrementing RAX from 3 to 0 | Passed | TESTED |
| CPU-009 | Run `cpu_sub_cmp_differential` on Linux x86-64 with GCC/Clang | Each of SUB and CMP matches native SUB flags for 20,108 cases, CMP preserves RAX | Passed in Debug, Release and ASan/UBSan | TESTED |
| STORAGE-001–040 | Run `storage_tests` using generated original fixtures; details in STORAGE_TEST_CASES.md | Range, error, CRC and synthetic integration checks pass | All 40 passed in Debug, Release and ASan/UBSan | TESTED |
| INPUT-001 | Run `input_tests` with synthetic devices/buttons | Connections, duplicates, actions, bindings, removal, rebinding and counts match expected state | Passed in Debug, Release and ASan/UBSan | TESTED |
| MEM-004 | Run `memory_model_tests`: 100 fixed seeds, 500 operations each against a separate 64-byte reference model | Status, readable/executable data, unchanged failed outputs and statistics match after each operation; each operation has successes and failures | Passed in Debug, Release and ASan/UBSan | TESTED |
| GRAPHICS-001 | Run `graphics_tests`: 100 fixed seeds, 200 command batches each against a snapshot-based reference | Pixel output and validation agree after every batch | Passed in Debug, Release and ASan/UBSan; 7,200 accepted and 12,800 rejected batches | TESTED |
| GRAPHICS-002 | Run explicit creation, overlapping copy, invalid batch and move-state checks | Bounds, snapshot semantics, transactional rejection and moved-from invariants hold | Passed in Debug, Release and ASan/UBSan | TESTED |
| GRAPHICS-003 | Run `nightfall_graphics_demo /tmp/nightfall-demo.ppm` on Linux | Original 320×180 P6 file written | Passed; 172,815 bytes | TESTED |
| RUNTIME-001 | Run `runtime_timer_tests`: ordering, cancellation, zero budget, backwards time, overflow, zero capacity | Correct event order/status, no state change on time errors | Passed in Debug, Release and ASan/UBSan | TESTED |
| RUNTIME-002 | Run 100 fixed-seed timer scenarios of 1,000 operations each against a sorted-vector model | Schedule, cancel, delivery, capacity and state agree after every operation | Passed in Debug, Release and ASan/UBSan | TESTED |
| INTEGRATION-001 | Run `system_integration_tests` using original input, timer, CPU, memory, software graphics and CRC data | Event and button trigger synthetic computation; generated pixels and CRC match | Passed Debug, Release and ASan/UBSan | TESTED |
| CPU-010 | Run `cpu_fault_tests` with unsupported REX-prefixed opcode | Invalid-instruction stop with unchanged CPU state | Passed Debug, Release and ASan/UBSan | TESTED |
| CPU-011 | Run `cpu_register_tests` | Immediate MOV for all 16 registers, truncation, unsupported memory form and sample sequence work | Passed Debug, Release and ASan/UBSan | TESTED |
| CPU-012 | Run `cpu_register_differential` on Linux x86-64 GCC/Clang | 32,768 register MOV/ADD/SUB/CMP cases agree with native arithmetic and preserve unrelated registers | Passed Debug, Release and ASan/UBSan | TESTED |
| CLI-001 | Run six `cli_raw_*` CTest cases | Original register, memory and stack raw demos return expected RAX; missing file, invalid budget and exhausted budget fail | Passed Debug, Release and ASan/UBSan | TESTED |
| CPU-013 | Run `cpu_memory_mov_tests` | 1,280 register/base/addressing combinations, unaligned access, protection faults, truncation and invalid forms behave as specified | Passed Debug, Release and ASan/UBSan | TESTED |
| CPU-014 | Run `cpu_address_stack_tests` | Selected SIB/RIP-relative addresses, all GPR PUSH/POP, RSP semantics, CALL/RET, faults and synthetic function call match expectations | Passed Debug, Release and ASan/UBSan | TESTED |
| LOADER-001 | Run `elf_loader_tests` on original constructed ELF data | PT_LOAD mapping, BSS zero-fill, permissions, stack and entry work; malformed inputs are rejected without modifying prior machine state | Passed Debug, Release and ASan/UBSan | TESTED |
| LOADER-002 | Run `cli_elf_demo` using original GNU-binutils-built fixture | Loads ELF64 and executes synthetic CALL/RET to HLT with RAX=42 | Passed Debug, Release and ASan/UBSan | TESTED |
| CPU-015 | Run `cpu_compiled_instructions_tests` | MOV imm32 zero extension, LEA address calculation, imm8, invalid forms and faults match expected behavior | Passed Debug, Release and ASan/UBSan | TESTED |
| CPU-016 | Run `cpu_imm8_differential` on Linux x86-64 GCC/Clang | 12,288 ADD/SUB/CMP imm8 cases match host flags and results | Passed Debug, Release and ASan/UBSan | TESTED |
| LOADER-003 | Run `cli_compiler_elf_demo` | Original freestanding C ELF compiled with GCC 12.2 executes to synthetic HLT with RAX=42 | Passed Debug, Release and ASan/UBSan | TESTED |
| CPU-017 | Run `cpu_jcc_tests` | 1,024 short/near conditional-branch and flag combinations match explicit truth tables; truncation faults preserve state | Passed Debug, Release and ASan/UBSan | TESTED |
| CPU-018 | Run `cpu_test_differential` on Linux x86-64 GCC/Clang | 8,192 TEST register cases match native defined flags; AF is excluded and preserved | Passed Debug, Release and ASan/UBSan | TESTED |
| LOADER-004 | Run `cli_loop_elf_demo` | Original freestanding C loop ELF executes to synthetic HLT with RAX=45 | Passed Debug, Release and ASan/UBSan | TESTED |
| CPU-019 | Run `cpu32_tests` | 32-bit zero extension, preserved upper bits on CMP/TEST, faults and near JMP work | Passed Debug, Release and ASan/UBSan | TESTED |
| CPU-020 | Run `cpu32_differential` on Linux x86-64 GCC/Clang | 15,360 32-bit register and imm8 cases match native defined flags/results | Passed Debug, Release and ASan/UBSan | TESTED |
| LOADER-005 | Run `cli_int_loop_elf_demo` | Original unsigned-32-bit C loop ELF executes to synthetic HLT with RAX=45 | Passed Debug, Release and ASan/UBSan | TESTED |
| CPU-021 | Run `cpu32_memory_tests` | 320 register/base/mode memory combinations, SIB, RIP-relative immediate, zero extension, permissions and faults match expected results | Passed Debug, Release and ASan/UBSan | TESTED |
| LOADER-006 | Run `cli_array_elf_demo` | Original freestanding C array ELF executes to synthetic HLT with RAX=42 | Passed Debug, Release and ASan/UBSan | TESTED |
| CPU-004 | Run `cpu_add_differential` on Linux x86-64 with GCC/Clang | ADD result and six flags match native register ADD for 108 edge and 20,000 deterministic cases; unrelated flags preserved | Passed in Debug, Release and ASan/UBSan | TESTED |

Environment for these tests: C++20 and CMake, no GPU or guest files. The first four checks share one CTest executable; mapping, reference-model memory, fault, branch, register, storage, input, graphics, runtime, system integration, ELF loader and native-oracle checks use separate CTest executables. Eleven CLI tests invoke the CLI. The native oracle is not built on unsupported platforms. This is not exhaustive. An initial synthetic benchmark is in PERFORMANCE.md. The 100 fixed-seed memory scenarios are a limited deterministic model test, not coverage measurement or a substitute for fuzzing. No Windows, physical input, extended stability, real-guest or fuzz tests performed. An initial GCC coverage measurement is in COVERAGE.md; it is not a measure of feature completeness.

## Run the current suite together
On Linux with GCC or Clang and CMake installed, run `./scripts/verify.sh`; it builds and runs Debug, Release, and ASan/UBSan variants and exits on failure. The six native differential executables are included only on supported Linux x86-64 GCC/Clang. On Windows, `powershell -File scripts/verify.ps1` is provided for Debug and Release with a multi-config MSVC generator; the script and Windows builds have not been tested here. GPU hardware, Windows, real game and future functionality require tests that cannot be completed in this environment.

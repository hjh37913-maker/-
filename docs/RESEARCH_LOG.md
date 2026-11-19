# Research log
2026-11-19: Sony's public hardware briefing establishes high-level system specs, but not guest interfaces; recorded source and boundaries in research/PS5_ARCHITECTURE.md. This is insufficient to implement a console loader or GPU. Selected portable, independently testable instruction and memory experiments rather than speculate about undocumented interfaces. GPU and Windows tests unavailable in the observed environment.

2026-11-19 continuation: Compared the implemented ADD-immediate result and arithmetic flags with native ADD over 20,108 deterministic cases on the available x86-64 host. This validates those cases only; console CPU behavior and the rest of x86 remain unresolved. Added and measured a synthetic ADD loop; see PERFORMANCE.md.

2026-11-19 continuation: Implemented experimental map/unmap/protect operations and permission-aware CPU fetch; these are research abstractions, not inferred PS5 interfaces. Tests exercise denied and cross-boundary accesses, full-range atomicity, zero-fill on remap, diagnostics and all truncated lengths of existing multi-byte instructions. Measured one synthetic post-change run; see PERFORMANCE.md.

2026-11-19 continuation: Added SUB/CMP RAX immediate and short JMP/JZ/JNZ. Native x86-64 differential testing passed 20,108 cases each for SUB and CMP, using native SUB as the flag oracle. Synthetic branch and integration tests passed; this does not establish console compatibility. Re-ran the ADD microbenchmark; see PERFORMANCE.md.

2026-11-19 continuation: Developed and tested a bounded synchronous storage primitive and incremental CRC-32 using original fixtures. Storage-to-memory-to-CPU integration runs only a synthetic instruction stream. Filesystem timing, asynchronous operation and PS5-specific I/O semantics remain unresearched and unimplemented.

2026-11-19 continuation: Added input state aggregation for injected events and a deterministic reference-model test for guest memory. Tested 100 seeds, 500 operations each, including both success and failure of map, unmap, protect, write, read and fetch. Synthetic input tests do not establish physical keyboard, controller or console support.

2026-11-19 continuation: Built a CPU reference surface and compared synthetic command batches with a snapshot oracle. The first test run exposed a bug in the test oracle: it used an 8×7 stride with a 4×4 surface. Corrected the explicit test to use matching dimensions, then reran Debug, Release and sanitizer tests successfully. A move-state invariant was also added and tested. This experiment establishes no PS5 graphics behavior.

2026-11-19 continuation: Added a standalone logical timer queue and verified it against a sorted-vector model over 100 deterministic scenarios. The initial random workload failed its coverage guard because capacity 32 was never reached; the test capacity was reduced to 4, after which successful and rejected schedules were both exercised. This tests project-defined behavior, not PS5 services.

2026-11-19 continuation: Ran all 12 registered tests in three Linux configurations and an instrumented GCC coverage build. Added an unsupported REX-opcode regression after coverage revealed that path had not executed. Library source-file line/branch counts and gaps are in COVERAGE.md; these numbers do not measure PS5 or game compatibility.

2026-11-19 continuation: Exercised register-only x86-64 encodings across all source/destination registers and both operand directions against native arithmetic, 32,768 deterministic cases. Added a 20-byte original raw program and verified bounded CLI execution. Repeated Debug, Release, ASan/UBSan and GCC coverage tests. None of this establishes PS5 executable or game compatibility.

2026-11-19 continuation: Added MOV data loads/stores with 8- and 32-bit sign-extended displacements, access checks and failure atomicity. Tested 1,280 legal register/address/direction combinations and a standalone raw memory example, with all 20 Linux CTest cases passing in Debug, Release and ASan/UBSan. Tests are synthetic and do not establish console behavior.

2026-11-19 continuation: Implemented SIB/RIP-relative MOV addressing and PUSH/POP/CALL/RET for original test programs. Tested stack-pointer edge semantics, faults, addressing examples and a synthetic function call; all 22 Linux tests passed in Debug, Release and ASan/UBSan. This does not establish PS5 or GTA VI compatibility.

2026-11-19 continuation: Implemented a generic ELF64 PT_LOAD experiment and tested both generated malformed inputs and an original GNU binutils ET_EXEC fixture. The first build exposed invalid lambda default arguments in the new test, corrected before rerunning Debug, Release and ASan/UBSan successfully. These tests establish nothing about PS5 guest formats, OS ABI, security or GTA VI.

2026-11-19 continuation: Compiled a freestanding original C function with GCC 12.2, inspected the generated x86-64 assembly, implemented required MOV imm32, LEA and ADD/SUB/CMP imm8 forms, and ran the resulting restricted ELF to synthetic HLT (RAX=42). Added 12,288 native imm8 comparisons; all 27 tests passed on Linux Debug, Release and ASan/UBSan. This demonstrates only that fixture, not Linux processes, PS5 software or GTA VI.

2026-11-19 continuation: Inspected GCC 12.2 output for a freestanding original C loop, implemented register TEST and all sixteen short/near Jcc conditions, and executed the compiled ELF to synthetic HLT with RAX=45. Checked 1,024 branch/flag combinations and 8,192 native TEST cases; all 30 tests passed on Linux Debug, Release and ASan/UBSan. No PS5 compatibility is implied.

2026-11-19 continuation: Inspected GCC 12.2 output for an original unsigned-32-bit C loop. Added the required 32-bit register operations and JMP rel32; the resulting freestanding ELF produced RAX=45. Checked 15,360 native differential cases and all 33 CTest cases passed on Linux Debug, Release and ASan/UBSan. This is not PS5 or game execution.

2026-11-19 continuation: Inspected GCC 12.2 output for an original freestanding C array example. Implemented required 32-bit memory loads, stores, immediate stores and LEA; the compiled test ELF produced RAX=42. Updated the earlier test that expected all 32-bit memory MOV forms to be invalid, and verified 320 addressing cases plus the full 35-test suite on Linux Debug, Release and ASan/UBSan. No console compatibility is claimed.

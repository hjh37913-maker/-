# Persistent project state — 2026-11-19

## Evidence-backed dashboard
| Area | State | Evidence |
|---|---|---|
| Research | Started | Public hardware specifications and open uncertainties in research/PS5_ARCHITECTURE.md |
| Architecture | Initial design | MASTER_ARCHITECTURE.md |
| Core | TESTED | Budgeted synthetic machine, CTest on Linux |
| CPU | TESTED, narrowly | 16 GPRs; register MOV/ADD/SUB/CMP, selected MOV memory, immediate and branch forms; native differential tests cover selected cases, not exhaustive |
| Memory | TESTED, narrowly | Flat and bounded sparse experimental memory, map/protect/unmap and R/W/X checks; not a console memory model |
| Storage | TESTED, narrowly | Synchronous bounded host-file ranges and CRC-32; 40 checks, no PS5 compatibility |
| Input | TESTED, narrowly | Core synthetic mapper; browser keyboard/mouse diagnostic only, no guest input |
| Graphics experiment | TESTED, narrowly | CPU-only surface, triangle rasterization and depth experiment; no GPU or PS5 compatibility |
| ELF64 test loader | TESTED, narrowly | Loads a restricted original x86-64 ELF64 fixture, not PS5 binaries |
| Runtime experiment | TESTED, narrowly | Deterministic timer and bounded synthetic Linux x86-64 test ABI; not PS5 or a host scheduler |
| GPU, shader translation | NOT_IMPLEMENTED | No compatibility implementation |
| UI | Implemented experimentally | Browser-local metadata library, browser input diagnostic, and real synthetic CLI test runner; not a game launcher |
| Performance telemetry | NOT_IMPLEMENTED | No real-game telemetry |
| Build | TESTED | Linux GCC Debug, Release and sanitizer builds; Windows x64 cross-compiled, runtime untested |
| Coverage | Measured, limited | 155 registered CTest cases on Linux x86-64; older gcov measurements in COVERAGE.md are not current or a feature-completeness metric |
| GTA VI | NOT_IMPLEMENTED | No loader, guest OS, graphics, legitimate delivery path, or game execution test |

## Current synthetic guest service experiment

Explicit guest-address bindings dispatch bounded SysV memory/string primitives
and an authored SCE-style synthetic file exercises a guest call at three load
biases. Linux Debug, Release and ASan+UBSan passed 155/155 tests before
additional fault assertions; those updated tests were rerun in all three.
Windows test cross-compiled only. This is not a verified PS5 system library,
PS5 file loader or GPU. Details: `../reports/nightfall-guest-services/WORK_REPORT.md`.

## Current synthetic raw-loader mutation coverage

Added deterministic malformed-input/rollback testing for the restricted
SCE-style raw loader. Linux Debug, Release, ASan+UBSan and a clean Release
passed 152/152 tests each. Windows x64 portable mutation tests were
cross-compiled but not executed. These inputs are synthetic and do not prove
PS5 or game compatibility or isolation.

## Current equivalent native import comparison

An original native Linux ELF importing a synthetic function and the synthetic
SCE-style guest case return the same value. The containers and linkers differ;
this is a comparison of equivalent logic, not a PS5 format or game test.

## Current explicit SCE-style dependency identities

The synthetic raw relocation API can validate caller-asserted module/library
names, IDs and versions and match encoded import qualifiers. Assertions do
not load or implement any module. Debug and Release 150/150 passed; the full
ASan+UBSan suite passed separately after the combined verification timed out.
Windows cross-compiled only. No real PS5 file or game was tested.

## Current candidate module/library analysis

The passive candidate-ELF inspector can report module/library declarations
and match encoded symbol qualifiers to unambiguous IDs; this is diagnostic,
not execution or PS5 compatibility. An independent arithmetic model covers
5,000 generated SCE-style relocation cases. Linux Debug, Release and
ASan+UBSan passed 150/150; Windows x64 cross-compiled, not run.

## Current restricted SCE-style raw relocation experiment

Added an opt-in C++ API to map and apply a narrow subset of OS-tagged
relocations to independently authored synthetic raw ELF data with explicit
guest bindings and transactional failure. Linux Debug, Release and ASan+UBSan
passed 149/149 tests. Windows CLI and portable test cross-compiled, not run.
No authentic PS5 executable or game was tested; this is not a PS5 launcher,
complete ABI or evidence GTA VI runs. See research/PS5_ARCHITECTURE.md.

## Current candidate relocation metadata inspection

For candidate raw SCE-style ELF files, the passive inspector also displays
bounded OS-tagged RELA/PLT-RELA records with checked symbol indices. No record
is applied and no game code runs. It has only synthetic fixtures, not a PS5
sample or validation against a console. Linux Debug, Release and ASan+UBSan
passed 148/148 each; Windows x64 cross-compiled, not run.

## Current candidate SCE-style import inspection

Added passive bounded metadata inspection for unprotected raw ELF headers and
OS-specific import table tags observed in a public PS5 project. Only synthetic
fixtures have been tested; output is not platform identification or support.
The inspector does not decrypt, resolve symbols, load code, or launch games.
Linux Debug, Release and ASan+UBSan passed 148/148 each; Windows CLI
cross-compiled but did not run on Windows. See research/PS5_ARCHITECTURE.md.

## Current missing-import diagnostics

The unprotected ET_DYN loader reports the first unbound guest import by name;
the CLI escapes control bytes. A 5,000-case deterministic mutation test
checks loader rollback/invariants, not semantic correctness or isolation.
Linux Debug, Release and ASan+UBSan passed 147/147 each; the Windows x64 CLI
cross-compiled but was not run. PS5 formats and GTA VI remain unsupported.

## Current unprotected ELF relocations and guest imports

A separate opt-in ET_DYN loader handles bounded RELATIVE and self-symbol
GLOB_DAT/64 relocations plus explicitly supplied guest-address-only bindings.
Six new tests run authored fixtures, include fail-closed rejection and rollback,
and compare three original cases with Linux native dynamic linking. Linux
Debug, Release and ASan+UBSan passed 146/146 tests each. Windows x64 was
cross-compiled only. This is not a PS5 loader or evidence of GTA VI support.
See `docs/ELF_LOADER.md` for restrictions.

## Current descriptor-flag batch

Modeled `F_GETFD`, `F_SETFD`, `F_DUPFD`, and `F_DUPFD_CLOEXEC`, with
per-descriptor close-on-exec flags and shared file offsets. Linux Debug,
Release and ASan+UBSan passed 140/140 tests; selected stateful comparisons
use private Linux memfds. Windows cross-compiled but not run. There is no
modeled exec and this is not PS5 or game support.

## Current file-fixture differential batch

Added a test-only runner and randomized differential driver for the original
compiled C file fixture. On Linux, 45,000 executions matched Python reference
and native temporary-file executions across five deterministic seeds; sizes
0–96 bytes include the intentional oversize error path. Linux Debug, Release
and ASan+UBSan passed 138/138 CTests. Windows runner cross-compiled only.
These are test files, not games or PS5 software.

## Current POPCNT batch

Added 16/32/64-bit POPCNT register/memory forms and portable fault tests.
Linux Debug, Release and ASan+UBSan passed 137/137 tests each; 98,304 native
register and status-flag comparisons passed on the test CPU. Windows x64 CLI
and portable tests cross-compiled, but were not run. This is not a CPUID model
or evidence of PS5/game compatibility.

## Current shared file descriptions and bit-scan batch

Modeled bounded dup/dup2/dup3 sharing open-file offsets, and x86 BSF/BSR
16/32/64. Linux Debug, Release and ASan+UBSan passed 135/135 tests each,
including Linux native register and private-memfd comparisons. Windows x64
cross-compiled only; no Windows runtime or PS5/game compatibility is proven.

## Current synthetic file-metadata batch

Added bounded x86-64 synthetic stat/fstat metadata for registered test files
and an original compiled C file fixture. Linux Debug, Release and ASan+UBSan
passed 131/131 tests each; Windows x64 cross-compiled only. Metadata and paths
are synthetic, not host access, PS5 support or game compatibility.

## Current synthetic read-only files batch

Added an opt-in, bounded in-memory file table and modeled descriptors,
open/openat, read/readv, pread64, lseek and close. Linux Debug, Release and
ASan+UBSan passed 130/130 tests; selected native tests use private synthetic
memfds. Windows x64 cross-compiled only. Guest paths never open host files;
this is not a sandbox, general Linux filesystem or PlayStation compatibility.

## Current bounded anonymous-memory batch

Added a deterministic, bounded anonymous mmap arena with mprotect/munmap for
test ELFs, portable and native checks, and an original static fixture. Linux
Debug, Release and ASan+UBSan: 127/127 tests each; Windows x64 cross-compiled
only. This is not a sandbox or a complete Linux or PS5 memory ABI. See
LINUX_TEST_ABI.md for strict limits and intentional differences.

## Current SSE2 integer and fetch-cache batch

Added selected XMM integer and bitwise moves/operations, explicit alignment
fault reporting, a compiled original vector ELF, and within-step instruction
fetch reuse. Linux Debug, Release and ASan+UBSan: 124/124 CTest tests each.
Windows x64 cross-compilation only; no vector ABI, GPU or PS5 compatibility is
claimed. See CPU_ARCHITECTURE.md and PERFORMANCE.md for exact evidence.

## Current REX32 and compiled ELF batch

Added selected REX-prefixed 32-bit instructions, multi-byte NOP decoding,
and an original GCC-compiled freestanding Linux hash test. 120/120 tests passed
in Linux Debug, Release and ASan+UBSan; Windows x64 cross-compiled only. The
hash fixture is not a game, crypto implementation or PS5 compatibility test.

## Current restartable string-instruction batch

Added step-bounded MOVS/STOS/LODS, CMPS/SCAS with REP/REPE/REPNE and
CLD/STD, native comparisons, partial-fault and budget checks, and an original
Linux ELF. Linux Debug, Release and ASan+UBSan passed 118/118 tests each;
Windows x64 cross-compilation only. This is not game or PS5 support. See
CPU_ARCHITECTURE.md for supported prefixes and restart limitations.

## Current CPU carry and conditions batch

Added selected ADC/SBB, CMOVcc and SETcc forms with native comparisons and
original Linux test ELFs. Linux Debug, Release, and ASan+UBSan: 115/115
tests each; Windows x64 was cross-compiled, not run. See CPU_ARCHITECTURE.md.
This is not PS5 or game compatibility.

## Latest CPU division batch

Added selected DIV/IDIV byte through qword forms, `divide_error` propagation,
and BSWAP; Linux Debug, Release and ASan+UBSan passed 111/111 CTest cases.
Selected native CPU results and isolated native fault signals were compared;
Windows was cross-compiled but not run. The modeled Linux runtime does not
deliver guest SIGFPE. See CPU_ARCHITECTURE.md for exact scope.

## Latest CPU multiplication batch

Added byte/word/dword/qword MUL and one-operand IMUL, two-/three-operand
IMUL, and selected accumulator sign extensions. Linux Debug, Release and
ASan+UBSan: 106/106 tests each; selected native differential and an original
Linux ELF test passed. Windows x64 cross-compilation only. This does not add
PS5 or GTA VI compatibility. See CPU_ARCHITECTURE.md for precise scope.

## Latest bounded Linux test ABI batch

Original static test ELFs can exercise a minimal startup stack, buffered read/write and vector I/O, bounded synthetic heap and exit through a modeled syscall trap. All 101 tests passed on Linux Debug, Release and ASan+UBSan. Native private-pipe comparisons cover selected cases only; Windows binaries were cross-compiled but not run. This is not a PS5 ABI, sandbox or game launcher. See LINUX_TEST_ABI.md.

## Continuation priorities
1. Expand differential testing to further instructions and more arithmetic cases, and verify fault/decoding semantics; avoid assuming undocumented guest CPU features.
2. Expand memory property tests to fuzzing; investigate scalable page/region mappings and precise fault reporting before any real guest use.
3. Research lawful executable format and ABI requirements using authorized public documentation and independently created programs; implement no protected package support.
4. Prototype explicit GPU command/resource semantics using original workloads; establish a measured backend baseline before translation claims.
5. Establish Windows CI or a real Windows test environment. Build a desktop UI only once it can truthfully expose live engine state.

No tests were performed on Windows or against PS5 software. No distribution or circumvention of protected content is included.

## Latest completed batch
SUB and CMP immediate, JMP/JZ/JNZ rel8, arithmetic flags, native differential tests, truncation checks, branch direction/fault tests, a synthetic bounded loop, and updated measurement and documentation. All six CTest executables passed on the Linux environment in Debug, Release and ASan/UBSan. Windows and PS5 software remain untested.

## Current storage batch
A bounded host-file reader, incremental CRC-32, original temporary fixtures, synthetic CPU/memory integration and 40 individually numbered checks were added. These are 40 **test checks**, not 40 completed subsystems. Seven CTest executables passed on Linux in Debug, Release and ASan/UBSan. Storage async I/O, protected content and Windows remain untested/unimplemented. See STORAGE_ARCHITECTURE.md and STORAGE_TEST_CASES.md.

## Current input and memory batch
Implemented a synthetic digital button/action mapper with connection state, rebinding and diagnostic counts; it does not detect physical devices. Added 100 reproducible memory-model scenarios with 500 operations each, plus input state tests. All nine CTest executables passed on Linux Debug, Release and ASan/UBSan. The 100 scenarios are tests, not 100 implemented features or objectives. See INPUT_ARCHITECTURE.md and TESTING.md.

## Current graphics batch
Added a bounded software RGBA surface with clear/fill/copy command batches, overlap semantics, synthetic PPM generator, reference-model testing, and an opt-in CPU benchmark. 100 deterministic scenarios of 200 batches each passed, alongside targeted edge cases. All ten CTest executables passed in Linux Debug, Release and ASan/UBSan. This is not GPU or console emulation.

## Current runtime batch and progress interpretation
Added an independent logical timer queue with capacity, cancellation, stable ordering, overflow checks and budgeted delivery. 100 deterministic reference-model scenarios of 1,000 operations each passed. All 11 CTest executables passed on Linux Debug, Release and ASan/UBSan. These tests are not 100 completed emulator features.

No evidence-based total completion percentage is available: the remaining PS5 and game-specific scope is unknown and milestones cannot be accurately weighted. Measurable acceptance evidence: **zero successful GTA VI executions demonstrated; zero actual game execution attempts**. This is not a measurement of total engineering effort or the probability of success. Windows execution, the actual guest ABI and GPU translation remain unverified.

## Current verification batch
Added a synthetic integration scenario joining input, logical timer, CPU, guest memory, CPU graphics and CRC. All 12 registered Linux x86-64 tests passed in Debug, Release and ASan/UBSan; initial gcov measurements and known untested paths are in COVERAGE.md. Linux one-command verification was run; the provided Windows verification script is untested. Future PS5 and GTA VI compatibility tests remain undefined or blocked by absent implementations and lawful access.

## Current CPU and raw-test batch
Added 16 GPR state, MOV immediate, and register-register MOV/ADD/SUB/CMP in selected encodings. Added a bounded raw test-program CLI and an original 20-byte example; this is not a guest executable loader. 32,768 register-form differential cases passed. All 18 registered CTest cases passed on Linux Debug, Release and ASan/UBSan. No Windows or GTA VI test was performed.

## Current CPU memory batch
Added 64-bit MOV loads/stores for selected base/displacement addressing forms, permission and fault checks, a raw memory example, and a synthetic benchmark. 1,280 register/address combinations passed, together with all 20 CTest cases in Linux Debug, Release and ASan/UBSan. At that stage, SIB, RIP-relative operands and PS5 execution remained unimplemented.

## Current stack and addressing batch
Implemented selected SIB and RIP-relative MOV addressing, register PUSH/POP and direct CALL/near RET. Added a raw original function-call demo and initialized RSP for raw test programs. All 22 CTest cases passed in Linux Debug, Release and ASan/UBSan. These are partial x86 primitives, not PS5 compatibility. Windows, GPU translation, guest ABI and GTA VI execution remain unverified.

## Current ELF test-loader batch
Added a bounded, staged ELF64 x86-64 ET_EXEC loader and CLI option. An original binutils-built ELF test program was loaded and executed to synthetic HLT with RAX=42. All 24 registered CTest cases passed on Linux Debug, Release and ASan/UBSan. It is not a PS5 loader, game compatibility test or OS implementation. See ELF_LOADER.md for restrictions.

## Current compiled-program batch
Added MOV r32/imm32 zero-extension, 64-bit LEA and register ADD/SUB/CMP imm8. The original freestanding GCC 12.2 C ELF fixture now executes through the experimental loader and returns 42. All 27 CTest cases passed in Linux Debug, Release and ASan/UBSan, including 12,288 native imm8 differential cases. This does not establish general C, Linux, Windows, PS5 or GTA VI compatibility.

## Current C loop and branch batch
Added register TEST and all sixteen short/near Jcc condition codes. An original GCC 12.2 freestanding C loop ELF executes to synthetic HLT with RAX=45. All 30 registered CTest cases passed in Linux Debug, Release and ASan/UBSan; this is not evidence of general Linux, Windows, PS5 or GTA VI compatibility.

## Current 32-bit instruction batch
Added unprefixed 32-bit register-direct MOV/ADD/SUB/CMP/TEST and imm8 ADD/SUB/CMP, plus JMP rel32. The original GCC 12.2 unsigned-32-bit loop fixture executes to synthetic HLT with RAX=45; 15,360 native differential cases passed. All 33 CTest cases passed on Linux Debug, Release and ASan/UBSan. No Windows or PS5 software was tested.

## Previous 32-bit memory batch
Added unprefixed 32-bit MOV loads/stores, immediate stores, LEA and a shared effective-address decoder. An original GCC 12.2 C array fixture executed to synthetic HLT with RAX=42. 320 register/address/mode combinations and all 35 CTest cases passed on Linux Debug, Release and ASan/UBSan. No Windows, PS5 or GTA VI execution has been verified.

## Current 32-bit memory arithmetic batch
Implemented selected unprefixed 32-bit ADD/SUB/CMP/TEST memory forms, including signed imm8 operations. 29,440 portable memory-vs-register cross-checks exercise results and flags, with permission and fault checks; this is not an independent native differential test. The original C fixture uses explicit inline assembly to exercise memory arithmetic and halts with RAX=42. All 37 CTest cases passed on Linux Debug, Release and ASan/UBSan. Windows and PS5/GTA VI remain untested.

## Previous XOR and native memory differential batch
Implemented unprefixed 32-bit XOR register and memory forms, and REX.W 64-bit register XOR. A GCC 12.2 freestanding C XOR fixture executes to synthetic HLT with RAX=42. Independent Linux x86-64 native differential tests cover 36,864 32-bit memory/register ADD/SUB/CMP/TEST/XOR cases, 4,096 32-bit XOR register cases, and 8,192 64-bit XOR register cases; imm8 memory forms are not covered by that native oracle. All 40 CTest cases passed in Linux Debug, Release, and ASan/UBSan. No Windows or PS5/GTA VI execution was tested.

## Previous 64-bit memory arithmetic batch
Implemented selected REX.W 64-bit ADD/SUB/CMP/XOR/TEST register-memory forms, sign-extended imm8 ADD/SUB/CMP and sign-extended imm32 MOV to memory/register. The original C fixture uses explicit inline assembly and halts with RAX=42. A native Linux x86-64 oracle passed 36,864 memory register-operand cases; 768 memory-immediate cases compare to the existing interpreter register form, not native. Permission, truncated-instruction, alias, extended-register and fault checks also passed. All 43 CTest cases passed on Linux in Debug, Release and ASan/UBSan. Windows and PS5/GTA VI remain untested.

## Previous 5k+ scenario batch: imm32 group 1
Implemented `81 /0,/5,/7` ADD/SUB/CMP with imm32 for unprefixed 32-bit and REX.W 64-bit register and memory forms, including 64-bit sign extension. 24,576 native-arithmetic comparisons cover these forms and varied operands; the native oracle uses equivalent register arithmetic rather than the same immediate instruction encoding. Portable tests cover permissions, truncated instructions, unsupported groups and RIP-relative addressing. An original GCC C fixture with explicit inline assembly halted at RAX=42. The new test initially had a temporary-array `span` compile error, corrected before verification. All 46 CTest tests passed in Linux Debug, Release and combined ASan/UBSan. Windows, PS5 and GTA VI remain untested. Scenario count is not a count of independent subsystems.

## Current execution foundation batch
Implemented x86-64 near indirect CALL/JMP and PUSH from register or memory, which enables an original freestanding C function-pointer fixture to halt with RAX=42. Added bounded machine/CLI diagnostics with attempted count, stop RIP and execute-accessible bytes for faults. 6,144 portable register scenarios plus memory, address and fault tests cover indirect operations; these are not native differential tests. All 50 CTest tests passed in Linux Debug, Release and ASan/UBSan. Original fixture is not a game or PS5 executable. No OS ABI, console loader, GPU, Windows validation or game launch exists.

## Experimental local dashboard
Implemented a browser-based library of metadata and optional HTTPS source links; it does not copy, install or download games. The dashboard can submit only original trusted raw/ELF test files to the existing CLI and displays its actual output. Keyboard/mouse events are displayed locally in the browser, not sent to guests. Python HTTP integration (six unittest methods, registered as one CTest test) and a simulated-DOM smoke test passed. All 51 CTest cases passed on Linux Debug, Release and ASan/UBSan. UI has not been visually tested in a browser or on Windows. PS5, GTA VI, game downloads and gameplay input remain unsupported.

## Platform demo builds
Cross-compiled a native Windows x64 GUI demo linked to the actual synthetic CPU core, and packaged a separate optional browser dashboard with the Windows CLI. Built and debug-signed an offline Android WebView **UI-only** APK with no emulator core. PE headers, APK signature, alignment and manifest were checked; Linux CTest passed 51/51 in Debug, Release and ASan/UBSan. Android generated asset offline/disabled-execution checks passed in jsdom. Neither Windows runtime nor Android install/device/browser behavior was tested; no PS5 or GTA VI support. See DEMO_BUILDS.md.

## 30-minute engineering batch (2026-11-19)
Added selected 32/64-bit SHL/SHR/SAR, INC/DEC, NOT/NEG, and 16-bit MOV
forms with original test programs and native/portable checks. Implemented
bounded, byte-permission sparse guest pages, integrated them into the ELF test
loader, and executed an original test ELF above 4 GiB using the new optional
CLI mode. Added bounded metadata-only inspection for unprotected ELF64.
Implemented independent CPU triangle and depth-buffer experiments with PPM
demos and software-reference tests. All 90 CTest tests passed in each Linux
Debug, Release and ASan/UBSan configuration. A separate long-running native
shift oracle uses a fixed binary snapshot and varied deterministic seeds;
see its recorded result for actual duration and scope. Windows x64 CLI and
portable tests were cross-compiled but not run. These experiments do not
implement the console OS, a PS5 loader, graphics driver, PS5 games, or GTA VI.

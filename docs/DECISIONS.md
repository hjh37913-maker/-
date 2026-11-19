# Decisions
2026-11-19: C++20 and CMake minimize dependencies and support a future Windows-native application. Choose a small interpreter first to provide observable state and error paths, not as a performance prediction or a complete execution engine. Use a fixed array rather than inventing a PS5 memory map. Use HLT only as a synthetic stop sentinel. Avoid placeholder subsystems or a misleading launch UI. Hardware and Windows validation deferred because neither was available.

2026-11-19: Added a deterministic native ADD differential oracle, restricted to Linux x86-64 with GNU-compatible assembly; unsupported environments omit the oracle rather than report a false pass. A separate opt-in synthetic microbenchmark establishes a narrow baseline without conflating it with game performance.

2026-11-19 continuation: Added byte-level mapping metadata to permit small deterministic permission experiments. This costs one extra byte per guest byte and linear validation per access, deliberately prioritizing simplicity over scale. Default RWX preserves the existing synthetic API; unmapped construction must be explicitly selected. These checks are not host-enforced protection and do not make guest execution safe. Revisit with a measured page/region design before allocating large address spaces.

2026-11-19 continuation: Added only specific RAX immediate arithmetic and rel8 branch encodings, rather than implying general x86 support. Shared ADD/SUB/CMP flag calculation is checked against host hardware; short-branch target validation remains at the next fetch, with no claim of architectural exception fidelity. SUB/CMP and branches permit a small original loop as an integration test.

2026-11-19 continuation: Added a synchronous host-file range primitive rather than claiming console storage compatibility. The caller must supply the maximum read length. Regular-file preflight is not treated as sandboxing because path lookup is inherently racy here. CRC-32 serves accidental-error detection only, with no cryptographic or DRM claims. Chose original temporary fixtures and a synthetic interpreter integration rather than loading proprietary content.

2026-11-19 continuation: Introduced a synthetic digital mapper without premature platform-specific device code. Control IDs intentionally share a namespace across devices and callers must normalize device-specific controls and serialize access. Added deterministic model-based memory testing before attempting a scalable memory redesign. The 100 scenarios are not counted as 100 independent implemented features.

2026-11-19 continuation: Added a CPU-only RGBA reference surface and synthetic command batches, rather than inventing PS5 command semantics or claiming a Vulkan/D3D backend. Commands are fully prevalidated and copies handle overlap without allocation. Surface is move-only; moved-from objects are reset to empty. This represents an original test workload, not GPU compatibility.

2026-11-19 continuation: Implemented a bounded, caller-driven logical timer queue rather than assume undocumented guest timer behavior. Ordered one-shot events and explicit budgeted draining make tests deterministic. Cancellation is linear and scheduling allocates; optimize only with measured workloads. No callback or OS synchronization semantics are implied.

2026-11-19 continuation: Added a one-command Linux Debug/Release/ASan+UBSan verification script and an untested Windows multi-config counterpart. Added a cross-component synthetic integration check and GCC coverage measurements to identify gaps rather than equating test count with completeness. Allocation-failure and short-read injection remain future work.

2026-11-19 continuation: Expanded CPU state to 16 x86-64 GPRs while retaining the existing RAX field. Implemented selected REX.W register-only MOV/ADD/SUB/CMP forms and rejected memory addressing rather than speculating. Added an explicitly bounded raw-file mode for original test programs; it does not parse executable formats, bypass security or provide a sandbox. The CLI marks only the input byte range executable.

2026-11-19 continuation: Implemented only simple base and base-plus-signed-displacement 64-bit MOV data accesses; SIB, RIP-relative and arithmetic memory forms remain explicit invalid instructions. CPU stepping now requires mutable memory. The raw-test CLI retains RX permissions on code but uses RW non-executable memory elsewhere so original data-memory experiments can run. This permission model is experimental, not a host sandbox or PS5 memory map.

2026-11-19 continuation: Extended the prior intentionally restricted MOV decoder to SIB and RIP-relative addressing and added selected stack operations. The raw test CLI supplies a synthetic RSP at the end of the bounded address space; this is not an emulated console ABI. Permission faults are returned before CPU state changes. No protected software or guest operating-system semantics were added.

2026-11-19 continuation: Implemented a restricted generic ELF64 x86-64 ET_EXEC test loader to move beyond raw bytes. Segment construction is staged and committed only on success, with bounded input, range checks and explicit stack. It is intentionally not a Linux process loader or PS5 format implementation; no protected packages or undocumented APIs are handled. Unknown non-load header types are ignored, so accepted input is not a compatibility guarantee.

2026-11-19 continuation: Selected instruction extensions based on the actual output of GCC 12.2 for a small independent freestanding C fixture, rather than claiming general native or guest application compatibility. Added only MOV r32 immediate, LEA r64 and register-only 64-bit group-1 imm8 ADD/SUB/CMP; other operand sizes, groups, memory forms and APIs remain explicitly unsupported.

2026-11-19 continuation: Extended conditional execution and TEST to support a specific compiler-generated C loop, not general x86 compliance. Because x86 leaves TEST's AF undefined, the interpreter deliberately preserves it and differential tests compare only defined flags. Both short and near Jcc conditions are tested against explicit truth tables.

2026-11-19 continuation: Added only unprefixed 32-bit register forms required by an independently compiled C loop, with correct zero-extension and defined-flag behavior. Memory operands and REX-extended 32-bit forms remain rejected. JMP rel32 was added independently. Native differential tests were added before compatibility claims.

2026-11-19 continuation: Refactored effective-address calculation into a shared CPU decoder to implement unprefixed 32-bit memory MOV and LEA without duplicating SIB/RIP-relative handling. Immediate bytes are included when calculating RIP-relative addresses. Arithmetic memory and 32-bit REX forms remain unsupported. This work enables only original synthetic/freestanding test programs, not PS5 binaries.

2026-11-19 continuation: Added selected unprefixed 32-bit memory arithmetic forms with staged CPU state for write faults. This models single-threaded instruction outcomes only: read-modify-write is not host-atomic and LOCK is not supported. Cross-checks against register forms are not independent native differential tests.

2026-11-19 continuation: Implemented 32-bit register/memory XOR and 64-bit register XOR, preserving undefined AF consistently with TEST. No LOCK or 64-bit XOR memory form. Added an independent native oracle for selected 32-bit memory register-operand arithmetic (not memory immediate forms), and both 32- and 64-bit register XOR. No change to synthetic HLT or console support.

2026-11-19 continuation: Implemented selected REX.W memory arithmetic/TEST/XOR, imm8 memory arithmetic, and MOV sign-extended imm32. State and flags are staged before guest-memory writes; this is not a host-atomic RMW or a model of LOCK. Memory immediate cases were cross-checked with interpreter register forms, not independent native execution.

2026-11-19 continuation: Added group-1 imm32 ADD/SUB/CMP in selected 32- and 64-bit register/memory forms. The 64-bit immediate is sign-extended. Memory writes remain staged and are not host-atomic. The 24,576-case oracle uses equivalent native register arithmetic, not hardware execution of identical guest bytes.

2026-11-19 continuation: Added near indirect CALL/JMP and PUSH for guest registers and memory as a foundation for compiled code using function pointers. Operand is read before stack updates; target instruction fetch happens at the next step. The model omits canonical-address checks and architectural exceptions. Added bounded diagnostics that report execute-accessible bytes only, without claiming a decode or identifying the faulting data address.

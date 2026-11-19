# PS5 architecture research

## Confirmed public information
Sony describes the original PS5 as an x86-64 Zen 2 CPU with 8 cores/16 threads, an RDNA 2-based GPU with ray tracing, 16 GB GDDR6 at 448 GB/s, and an 825 GB SSD rated at 5.5 GB/s raw. Frequency is variable. These are product specifications, not a complete programming model. [Sony hardware briefing](https://blog.playstation.com/2020/03/18/unveiling-new-details-of-playstation-5-hardware-technical-specs/) and [Sony FAQ](https://blog.playstation.com/2020/11/09/ps5-the-ultimate-faq/).

## Assumptions and implications
An x86-64 host may eventually allow native execution of some guest instructions, but guest ABI, exceptions, memory protections, concurrency, SIMD availability and security boundaries need independent validation. An interpreter provides a reference, not guaranteed performance. CPU/GPU sharing does not imply host VRAM and RAM are interchangeable; explicit coherency and synchronization models will be needed. Storage throughput alone says nothing about proprietary I/O behavior.

## Unknown / limitations
Exact exposed instruction feature set, guest virtual memory layout and kernel ABI, command formats, shader ISA variants, graphics synchronization and resource semantics, decompression interfaces, and game-specific requirements are not established here. Public general AMD documentation cannot establish console-specific behavior. No console, SDK, guest software, measured hardware behavior or proprietary documentation was accessed. Further lawful documentation and original test programs are required; no bypass of access controls is planned.

### CPU stack-frame experiment (2026-11-19)
The x86-64 interpreter now implements unprefixed `PUSH imm8`, `PUSH imm32`
(sign-extended to 64 bits), `LEAVE`, and `RET imm16`. Tests cover boundary
immediates, register/flag behavior, truncated instructions and stack faults.
These are generic instruction semantics for original test programs, not a PS5
ABI, game loader, graphics implementation, or evidence of game compatibility.
No native CPU differential test for these new instructions was performed.

### Logical operations experiment (2026-11-19)
Added unprefixed 32-bit and REX.W 64-bit register/memory AND and OR forms
(opcodes 09/0b/21/23). Immediate forms and non-REX.W extended 32-bit registers
are not implemented. AF is undefined by the ISA and preserved by this model.
On Linux x86-64, 36,864 register and memory results and defined flags were
compared against native CPU execution, alongside portable checks and faults.
The first regression run exposed a stale unsupported-opcode test; it now
checks a still-unsupported opcode and separately checks a truncated AND.
This generic instruction support does not implement the PS5 runtime or make
any game compatible.

### Logical immediates experiment (2026-11-19)
Added group-1 OR/AND/XOR immediate forms for 32/64-bit registers and memory.
49,152 results and defined flags are compared with equivalent native register
operations, not the same instruction encoding or guest addressing. Portable
checks cover short fetch and write faults. An initial test build failed due to
an assembly-test macro naming collision; after fixing that, four existing
unsupported-opcode checks failed because their opcodes are now implemented.
Those checks now use unsupported ADC, preserving the original assertions;
full regressions passed. This does not add a PS5 ABI or game compatibility.

### Byte and word reads (2026-11-19)
Implemented the x86 MOVZX byte/word source forms, including REX and high-byte
register semantics. Native Linux x86-64 comparisons cover memory sources;
portable checks cover register encodings and faults. This enables an original
freestanding C byte-read fixture, not PS5 binaries or games.

### Signed extension and byte transfers (2026-11-19)
Added MOVSX byte/word and REX.W MOVSXD, byte MOV register/memory/immediate,
and accumulator logical immediate instructions. Portable tests check aliases,
legacy high-byte and REX low-byte registers, memory permissions and faults.
Linux x86-64 native checks cover 40,960 sign-extension memory cases and
32,768 byte memory transfers. 49,152 accumulator-immediate comparisons use
native register operations as oracle rather than the immediate encoding.
Three original freestanding C fixtures exercise these operations; an integration
check validates byte copies in guest memory. Windows x64 tests cross-compiled,
but could not be run on Windows. No PS5 operating system, graphics stack or
game support is implied.

### Upstream project review (2026-11-19)
The public [KytyPS5 repository](https://github.com/KytyPS5/KytyPS5) describes
itself as a PS5 translation layer targeting Windows/Linux with an experimental
macOS port; its source is GPL-2.0. Its documentation describes substantial
loader, service, GPU and shader work beyond NIGHTFALL. This is a project claim,
not independent proof of GTA VI compatibility. A search on 2026-11-19 found
[an issue](https://github.com/KytyPS5/KytyPS5/issues/2404) asking to test GTA VI
without an actual game test, build, logs or reproducible result; it does not
establish either compatibility or incompatibility. No game or protected file
was obtained or executed, and no upstream source code was copied. NIGHTFALL's
new generic unprotected ELF relocation/import work is not a PS5 loader.

### Passive candidate import-table inspection

As a small PS5-directed research step, `--inspect-sce-imports` reads only
raw, unprotected little-endian x86-64 ELF metadata. The candidate OSABI,
object/header types and OS-specific table tags are factual values observed in
[KytyPS5's public header](https://github.com/KytyPS5/KytyPS5/blob/5ad27cdd1aeb48a7885b1a71d5a8321590b31ecd/src/loader/elf.h)
and its [runtime linker](https://github.com/KytyPS5/KytyPS5/blob/5ad27cdd1aeb48a7885b1a71d5a8321590b31ecd/src/loader/runtimeLinker.cpp),
reviewed 2026-11-19. The implementation is original and intentionally limited;
no upstream source code was copied. These values are *candidate* hints, not
proof that a file targets PS5 or is valid, authorized, or compatible. This
has only been tested using independently authored synthetic metadata; no
actual PS5 executable or game was available for verification.

The inspector reads bounded program headers, a bounded dynamic table, the
OS-specific symbol/string tables relative to the declared metadata segment,
and lists encoded names of undefined global/weak symbols. When present,
OS-tagged RELA and PLT-RELA records are reported as raw relocation type,
symbol index, target and addend; table sizes and indices are bounded. The
records are never applied. The inspector decodes compact numeric qualifier
tokens and matches them to declared module/library IDs and versions where
unambiguous; missing, invalid or duplicate IDs remain unmatched. It does not
decode or hash NIDs and does not resolve imports for execution. Names are escaped in CLI output. It
rejects non-ELF input outright and never parses SELF or other protected
containers, decrypts data, loads segments, or runs code. It is not a PS5
loader, compatibility verdict, security boundary, or indication GTA VI runs.
Ranges and counts are capped; the host file can change between individual
reads, and allocation errors can propagate. Example for a trusted raw test
file: `nightfall_cli --inspect-sce-imports fixture.elf`.

### Restricted raw SCE-style relocation experiment

`load_sce_raw_relocatable` adds an explicit C++ research API for a very small
subset of raw ELF metadata patterned after the public constants cited above.
It stages bounded PT_LOAD mappings, parses OS-tagged symbol/string and RELA
or PLT-RELA metadata, applies selected x86-64 RELATIVE, 64, GLOB_DAT and
JUMP_SLOT relocations, and resolves imports only against a caller-provided
name-to-guest-address table. It never calls or resolves host functions.
Unknown program headers/dynamic tags, module dependencies, TLS, missing
bindings, and unsupported relocations fail without committing machine state.
The sole executable scenario is a **synthetic test constructed from scratch**,
with a synthetic guest stub and a Linux test exit, checked at three load biases
on both memory backends. The API is not exposed as a PS5 game launch mode.

This is not a full PS5 loader, ABI, dynamic linker, sandbox, or a validated
implementation of console behavior. No authentic PS5 executable, protected
container, game, firmware, decryption or keys were used. Actual platform
program headers, module/library resolution, initialization, thread-local
storage, exceptions, OS services, GPU/shaders, and game behavior remain
unimplemented or unverified. Acceptance of a raw file in this subset does not
identify its platform or establish validity/compatibility.

The SCE-style relocation arithmetic model exercises 5,000 generated cases
per test over all four modeled relocation types, randomized load biases,
values and addends; this is a software reference, not a native PS5 oracle.

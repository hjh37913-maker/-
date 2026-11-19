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

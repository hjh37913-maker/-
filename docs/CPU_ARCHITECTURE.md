# CPU experiment

The interpreter recognizes these specific encodings only:

- `90` NOP and `f4` HLT, used as an experimental stop sentinel rather than a faithful privileged instruction.
- Unprefixed `89/8b/01/03/29/2b/31/33/39/3b/85` register-direct forms: 32-bit MOV/ADD/SUB/XOR/CMP/TEST over the eight legacy registers. Arithmetic and MOV write a zero-extended result; CMP and TEST preserve the full register. Other 32-bit REX forms are not supported except MOVZX as described below.
- Unprefixed `89/8b` MOV between r32 and m32, `c7 /0 imm32` MOV to r32/m32, and `8d` LEA r32: support base, SIB, displacement and RIP-relative memory addressing; r32 writes zero-extend, LEA truncates to 32 bits. `c7` RIP-relative addressing uses the end of the full instruction including its immediate. No 32-bit REX extensions are implemented.
- Unprefixed `01/03/29/2b/31/33/39/3b/85` with memory ModRM: 32-bit ADD/SUB/XOR/CMP in both directions and TEST, using the same effective addressing as MOV. Unprefixed `83 /0,/5,/7 imm8` and `81 /0,/5,/7 imm32` support register and memory operands (imm8 is sign-extended to 32 bits). Read-modify-write memory forms require read and write permission; failed reads or writes do not commit flags or CPU state. CMP and TEST only read. These operations are not host-atomic and do not implement LOCK.
- `b8..bf imm32` and `41 b8..bf imm32`: MOV immediate to a 32-bit register, zero-extending into the corresponding 64-bit register.
- `48..4f c7 /0 imm32`: MOV sign-extended imm32 to r64/m64; RIP-relative addressing uses the end of the complete instruction.
- `48/49 b8..bf imm64`: MOV immediate to any of the sixteen 64-bit general-purpose registers (REX.B extends the register).
- `48..4f 83 /0,/5,/7 imm8` and `81 /0,/5,/7 imm32` register and memory forms: ADD/SUB/CMP with sign-extended immediate.
- `48 05/2d/3d imm32`: ADD/SUB/CMP RAX with a sign-extended immediate.
- `48..4f 89/8b/01/03/29/2b/39/3b modrm`: MOV, ADD, SUB, CMP between 64-bit registers, in both encoded operand directions. ModRM must have `mod=11`; REX.R and REX.B select extended registers. REX.X has no effect on register operands; legacy prefixes are not supported.
- `48..4f 31/33` XOR on 64-bit registers or memory, including REX.R/B extensions. XOR clears CF/OF and updates ZF/SF/PF, preserving undefined AF in this model.
- `48..4f 8d modrm`: LEA with the same effective address calculation as supported memory MOV, without accessing memory; register-direct ModRM is invalid.
- `48..4f 89/8b modrm`: 64-bit MOV between registers and memory with base, SIB index/scale, signed 8/32-bit displacement, or RIP-relative addressing. Address arithmetic wraps modulo 2^64. Selected 64-bit arithmetic, XOR and TEST memory operands use the same address calculation. Reads and writes require their respective guest permissions and failures leave CPU state and memory unchanged. Unaligned accesses are supported by this model.
- `50..5f` and `41 50..5f`: 64-bit register PUSH/POP, including RSP semantics; `e8 rel32` CALL and `c3` near RET use the guest stack. `ff /2,/4,/6` with optional REX support near indirect CALL/JMP and PUSH from r64/m64, with operands evaluated before stack changes. Memory operands support SIB and RIP-relative addressing. Far calls/jumps, 16-bit forms and architectural canonical-address checks are not implemented. They preserve flags; stack read/write faults leave CPU state unchanged. Call/return targets are checked on subsequent instruction fetch.
- `48..4f 85 /r` TEST between a 64-bit register and register or memory. CF and OF are cleared; ZF, SF and PF follow the AND result. AF is undefined by x86 and is preserved by this model.
- `eb rel8` and `e9 rel32` JMP, `70..7f rel8` and `0f 80..8f rel32` for all sixteen conditional branches. The displacement is sign-extended from the end of the instruction. Target arithmetic wraps as unsigned 64-bit; invalid target access is reported on the next fetch. Canonical-address checks and architectural exceptions are not modeled.

ADD/SUB/CMP update CF/PF/AF/ZF/SF/OF, preserving other modeled flag bits. CMP and TEST do not change their operands; MOV and branches do not change flags. State represents 16 general-purpose registers, RIP and RFLAGS; `reg(index)` follows x86 encoding order and returns null for indices outside 0–15. Decode and memory faults leave state unchanged. Instruction bytes require execute permission independently of read permission; this is an experimental abstraction, not a claim about console faults. Other than the explicitly listed MOVZX sources, 8/16-bit registers and 32-bit REX forms remain unsupported; also unsupported are far CALL/RET and other indirect control-flow variants, full stack instruction set, modes, architectural exceptions, SIMD, floating point, threading, or PS5 CPU feature model are implemented.

On Linux x86-64 with GCC/Clang, native differential tests compare immediate ADD/SUB/CMP against equivalent native register arithmetic: 20,108 cases per operation. Register-form MOV/ADD/SUB/CMP have 32,768 native-reference cases over every source/destination register and both encodings. The 64-bit imm8 ADD/SUB/CMP forms have 12,288 Linux native-reference cases covering all immediate byte values and all 16 destination registers; 32-bit register and imm8 forms have 15,360 native-reference cases. A further 320 32-bit and 1,280 64-bit portable MOV memory combinations check both directions, supported registers, displacements and access faults against expected data; they are not a native differential test. The original 29,440 portable 32-bit memory arithmetic cross-checks compare against interpreter register forms, not an independent oracle. Separate Linux x86-64 native tests exercise 36,864 32-bit and 36,864 64-bit memory register arithmetic/TEST/XOR cases (no memory immediate forms), 4,096 32-bit register XOR cases and 8,192 64-bit register XOR cases against native instructions. The 24,576 group-1 imm32 cases compare both register and memory forms to native register arithmetic with the decoded equivalent operand; the native oracle does not execute the same immediate instruction encoding or guest address calculation. Fault and permission cases are also checked. TEST has 8,192 native-reference cases excluding architecturally undefined AF. Indirect control flow has 6,144 portable register scenarios plus memory and fault cases; these are synthetic, not native differential. SIB, RIP-relative, stack and call tests are synthetic rather than native differential. Branch conditions are checked against explicit truth tables for 1,024 short/near combinations, not against a native branch oracle. Tests are not exhaustive. See TESTING.md.

Interpretation is simplest for validation but likely slow. Dynamic translation needs validated decoding, invalidation, host ABI and isolation. Native execution can be fast on compatible CPUs but demands careful protection and exception handling; a hybrid needs all of those. Synthetic interpreter benchmarks are documented in PERFORMANCE.md; they are not a comparison with another execution strategy.

Additional supported forms: unprefixed `09/0b/21/23` operate on 32-bit
legacy registers and memory; `48..4f 09/0b/21/23` operate on 64-bit
registers and memory including REX extensions. These implement OR and AND
in both operand directions. Defined flags match logical TEST; undefined AF is
preserved. Memory write faults leave state unchanged. LOCK and non-REX.W 32-bit extended registers remain unsupported. A Linux x86-64
native differential test covers 36,864 register and memory cases, including
both widths and operand directions. These instructions do not provide PS5
compatibility.

Group-1 `81` imm32 and `83` imm8 now also support logical OR /1, AND /4,
and XOR /6 for unprefixed 32-bit and REX.W 64-bit register and memory
operands. The 8-bit immediate sign-extends for both widths; imm32 sign-extends
for 64-bit operands. Undefined AF is preserved; writes to 32-bit registers
zero-extend. ADC /2, SBB /3 and LOCK remain unsupported. 49,152 cases compare
the result and defined flags to equivalent native **register** instructions;
the oracle does not execute the same immediate encoding or guest addresses.
Portable checks cover truncation, RIP-relative operands and write failures.

`0f b6/b7` MOVZX supports 8/16-bit register and memory sources with
unprefixed or REX-prefixed 32/64-bit destinations. AH/CH/DH/BH and REX
low-byte encodings are distinguished. REX extends the source and destination
registers and applies to memory addressing. It preserves flags and faults
without changing state. Operand-size prefixes, 16-bit destinations and
other 8/16-bit instructions are not implemented. Portable tests cover all
REX/register encodings, aliases, RIP-relative reads and fault paths. Linux
x86-64 tests additionally compare 16,384 memory cases with native MOVZX.

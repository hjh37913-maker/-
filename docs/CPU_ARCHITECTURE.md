# CPU experiment

The interpreter recognizes these specific encodings only:

- `90` NOP and `f4` HLT, used as an experimental stop sentinel rather than a faithful privileged instruction.
- Unprefixed `89/8b/01/03/29/2b/31/33/39/3b/85` register-direct forms: 32-bit MOV/ADD/SUB/XOR/CMP/TEST over the eight legacy registers. Arithmetic and MOV write a zero-extended result; CMP and TEST preserve the full register. Other 32-bit REX forms are not supported except the extension instructions described below.
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

ADD/SUB/CMP update CF/PF/AF/ZF/SF/OF, preserving other modeled flag bits. CMP and TEST do not change their operands; MOV and branches do not change flags. State represents 16 general-purpose registers, RIP and RFLAGS; `reg(index)` follows x86 encoding order and returns null for indices outside 0–15. Decode and memory faults leave state unchanged. Instruction bytes require execute permission independently of read permission; this is an experimental abstraction, not a claim about console faults. Only the listed byte, word and 32-bit REX forms are supported. Far CALL/RET and other indirect control-flow variants, the full stack instruction set, modes, architectural exceptions, SIMD, floating point, threading, and a PS5 CPU feature model are not implemented.

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

`0f be/bf` MOVSX sign-extends byte or word sources to 32/64-bit destinations;
`REX.W 63` MOVSXD sign-extends 32-bit sources to 64 bits. Byte high-register
versus REX low-register semantics and memory addressing match MOVZX. These
instructions preserve flags and fault before modifying state. Non-REX.W `63`
and operand-size override forms are not implemented. Portable register/fault
tests and 40,960 native CPU memory cases cover these forms.

`88/8a` MOV support byte register-to-register and memory forms; `c6 /0`
supports immediate byte to register or memory. Optional REX extends registers
and selects low-byte registers instead of AH/CH/DH/BH. The untouched bits of
a register are preserved, flags are unchanged, and accesses are permission
checked. Portable tests cover encodings, aliases, RIP-relative immediates and
faults; 32,768 native memory cases cover byte transfers including AH.

`0d/25/35` and `48 0d/25/35` implement accumulator OR/AND/XOR immediate
forms for 32/64-bit destinations. Their defined flags follow the other logical
instructions; undefined AF is preserved. 49,152 comparisons use native
register operations as the oracle, not native immediate encodings.

## Later instruction expansion
`D1/D3/C1 /4,/5,/7` implement SHL/SHR/SAR for r/m32 and r/m64, using
implicit 1, CL or immediate counts with x86 5/6-bit masking and optional REX
register extensions. CF/PF/ZF/SF are defined for nonzero counts; OF is
updated only for count one; undefined AF and OF for larger counts are
preserved by this model. Count zero preserves flags, and a 32-bit register
destination still zero-extends. Memory RMW requires write permission even
for zero count in this model; atomic LOCK is unsupported. Native tests compare
equivalent register shifts, not the immediate encoding or guest addresses.

`FF /0,/1` support INC/DEC r/m32 and r/m64, preserving CF; `F7 /2,/3`
support NOT/NEG r/m32 and r/m64. `66 89/8b/c7 /0` and `66 b8..bf` support
16-bit MOV forms, with optional non-W REX and preserved upper register bits.
REX.W following 66 is currently rejected, not modeled as the 64-bit form.
Tests cover permission faults and compare equivalent native register or
memory operations as documented in the individual test sources.

## Multiplication and accumulator extension experiment

- `F6 /4,/5` provide one-operand MUL/IMUL on 8-bit register or memory
  sources; the 16-bit result is written to AX. Legacy AH..BH sources and
  REX-selected low-byte/extended sources are distinguished.
- `F7 /4,/5` provide one-operand MUL/IMUL on 16/32/64-bit register or
  memory sources, producing DX:AX, EDX:EAX or RDX:RAX. Portable unsigned
  limb arithmetic supplies the 64-bit high half without compiler-specific
  128-bit types. Source values are staged before destination writes.
- `0F AF`, `69`, `6B` implement two-/three-operand signed IMUL for 16/32/64
  bits, with register or memory sources and sign-extended immediates. The
  16-bit form preserves upper register bits; the 32-bit form zero-extends.
- CF and OF signal truncation; undefined PF/AF/ZF/SF are preserved by the
  model and must not be relied on. Fetch or source faults leave state intact.
  `66` combined with REX.W is rejected rather than treated as a 64-bit form;
  other prefix combinations are not a general decoder implementation.
- Exact encodings `98`, `66 98`, `48 98` implement CWDE, CBW and CDQE;
  `99`, `66 99`, `48 99` implement CDQ, CWD and CQO. Flags are preserved;
  arbitrary redundant REX/prefix combinations are not supported.

On Linux x86-64 the independent native instruction tests include 458,752
one-operand MUL/IMUL comparisons (including all 65,536 byte input pairs
for each signedness and guest source form), 196,608 selected two-/three-
operand IMUL comparisons and 98,304 accumulator-extension comparisons.
Guest register/memory variants are compared to equivalent native operands;
this does not validate host address decoding or every immediate encoding.
Portable tests additionally cover selected instruction/permission faults,
RIP-relative addressing, REX extensions and register aliases. An original
static Linux test ELF checks multiplication, sign extension, output and exit
against native execution. Windows was cross-compiled, not executed.

## Division and byte-order experiment

`F6 /6,/7` and `F7 /6,/7` implement unsigned DIV and signed IDIV of
register/memory operands in 8, 16, 32 and 64 bits. Signed quotient truncates
toward zero and the remainder follows the dividend's sign. Byte results go to
AL/AH; word results preserve upper RAX/RDX; dword results zero-extend.
Operands are staged before writes, including divisor aliases of accumulator
registers. Division by zero or quotient overflow yields `divide_error`,
leaving RIP, registers and flags unchanged; instruction/data read faults are
`memory_fault`. DIV/IDIV flags are architecturally undefined; this model
preserves them. The machine and Linux test runtime propagate the modeled
error, but they do **not** deliver CPU exceptions or Linux signals to a guest.

The implementation uses unsigned long division for wide dividends, avoiding
nonportable 128-bit host integers, with a checked host-division fast path when
the upper dividend half is zero. The nonzero divisor is checked first, and
that path's quotient necessarily fits the unsigned host width. Operand-size
prefix plus REX.W and the `66 F6` byte form are rejected; other redundant
prefixes are not a full x86 prefix decoder.

`0F C8..CF` and REX forms implement BSWAP r32/r64, preserving flags and
zero-extending 32-bit writes. Operand-size-override BSWAP is not implemented.

Native Linux x86-64 tests compare 98,491 successful DIV/IDIV cases against
actual CPU instructions and classify 163,653 selected divide errors using an
independent 128-bit integer reference. Sixteen deliberate native zero/overflow
cases run in isolated children with core dumps disabled and are checked for
SIGFPE. An optional exhaustive byte test covers every AX/divisor combination:
12,566,273 successes compared with native execution and 20,988,159 faults
classified with a separate bounded integer reference. Native BSWAP comparison
covers 131,072 inputs. These counts are test cases, not CPU completeness.
The source registers and memory forms exercised by guest tests may be compared
against equivalent native memory operands, not identical address encodings.
A self-authored Linux ELF exercises successful division; a separate explicit
fault fixture is run only under the model or an isolated native child.

## Carry arithmetic and conditional moves

`10..15`, `18..1D` and group `80/81/83 /2,/3` implement ADC/SBB
register, memory, accumulator and immediate forms in 8/16/32/64-bit widths.
They consume CF and update CF/PF/AF/ZF/SF/OF. All results and flags are
staged before a destination memory write; memory faults do not commit state.
Legacy high-byte versus REX byte-register semantics, sign-extended imm8 and
sign-extended imm32 for qword forms are supported. `66` with REX.W and
redundant `66` on byte operands are rejected, not generally decoded.

`0F 40..4F` CMOVcc and `0F 90..9F` SETcc use all sixteen existing condition
codes. CMOV supports 16/32/64-bit register and memory sources. The memory
source is read even when the condition is false; a false 32-bit CMOV also
zeroes the destination's upper half in long mode. SET writes a single byte,
including AH..BH versus REX low/extended registers, without altering flags.
Operand-size override SETcc and `66` plus REX.W are not supported.

Tests include 393,216 native ADC/SBB arithmetic comparisons across selected
guest forms, and 49,152 native CMOVcc/SETcc comparisons spanning all 16
conditions and all combinations of relevant flags. The arithmetic oracle uses
equivalent native register operations, not the exact guest addressing or
immediate encoding. Original Linux ELF fixtures exercise carry and conditional
instructions. Linux tests do not establish Windows or PS5 runtime support.

## Restartable string instructions

`MOVS`, `STOS`, `LODS`, `CMPS`, and `SCAS` support byte, word, dword and
qword element sizes with 64-bit RSI/RDI addressing, and `CLD`/`STD` change
DF. `F3` repeats all five; `F2` repeats CMPS/SCAS until equality (REPNE),
while F3 on CMPS/SCAS repeats while equal (REPE). Operand-size override and
REX.W are supported for word/qword non-byte forms, but not mixed; address-size
override, segments, arbitrary prefix combinations, and F2 MOVS/STOS/LODS are
not implemented. Comparison updates the arithmetic flags; MOVS/STOS/LODS
preserve flags. Guest memory permissions are checked before a step commits.

A repeated instruction executes **at most one element per CPU step** and
leaves RIP pointing at itself until done. Zero count consumes one step without
memory access. Each element consumes one execution-budget unit, so enormous
RCX values cannot block the host. On a fault, prior completed iterations
remain committed; the faulting iteration leaves its registers and flags
unchanged. This differs from retired-instruction counting. Self-modifying code
that changes the REP instruction during its execution can be re-decoded on
resumption; this experimental model is not a complete x86 exception/interrupt
implementation or a security boundary.

Native x86-64 tests compare 24,576 MOVS/STOS/LODS and 32,768 CMPS/SCAS
scenarios, including direction, overlap, stop conditions and flags. An original
Linux ELF exercises a bounded copy and search. These are selected cases, not
proof of full string-instruction or PS5 compatibility.

## REX-prefixed 32-bit forms and multi-byte NOP

Selected previously unprefixed 32-bit MOV, LEA, arithmetic, logic, TEST and
immediate-group forms now also accept REX prefixes with W=0. REX.R/B extend
register operands and REX.X/B extend effective address components; register
writes zero-extend. Compare and TEST retain their input registers. Faulting
memory RMW operations stage flags and destination values before write.
`0F 1F /0` multi-byte NOP accepts one optional `66` and one optional REX,
decodes ModRM/SIB/displacements but does not read the computed data address.
Other prefix combinations and unsupported instruction forms are not implied.

Linux native tests compare 114,688 selected R8D/R9D arithmetic operations;
30,720 portable cases compare prefixed and unprefixed register semantics
against each other, not an independent oracle. Separate tests cover extended
SIB addressing, invalid/truncated NOPs and faulting memory. An original GCC
12.2 `-O2 -nostdlib -static` Linux ELF computes FNV-1a from up to 64 input
bytes and prints a hex result: 71 sizes are checked against an independent
Python reference and native execution. FNV-1a is not cryptographic. No claim
of general C execution or PlayStation/Windows compatibility follows.

# Experimental ELF64 loader

This is a deliberately narrow research loader for independently authored, unprotected x86-64 ELF64 test programs. It is **not** a PS5 executable loader, Linux compatibility layer, sandbox, or a way to run games. The generic ELF concepts of PT_LOAD segments, file size versus memory size, and segment permissions are described in the [ELF manual](https://man7.org/linux/man-pages/man5/elf.5.html); this does not establish PS5 behavior.

## Supported behavior
The loader accepts little-endian ELF64 `ET_EXEC` for machine x86-64 with a bounded program header table. It validates file and address ranges, segment alignment, no overlapping load regions, and a runnable entry point. For each nonempty PT_LOAD it copies the file portion, zero-initializes the remaining memory, and applies R/W/X flags. A caller-selected, non-overlapping read/write stack is mapped; entry RIP and stack RSP are initialized. Load errors leave the original machine state unchanged: mapping is staged before commit. On success previous machine state is replaced.

`--run-elf` reads at most 1 MiB into a 1 MiB synthetic address space with the top 64 KiB reserved as stack, then executes with an explicit instruction budget (10,000 by default, at most 1,000,000). Example: `./build/nightfall_cli --run-elf examples/elf-demo.elf 5`. The example is an original static binary assembled from `examples/elf-demo.s`; expected RAX is 42. A second original fixture, `compiler-demo.elf`, was compiled from C and also returns 42; it is freestanding and relies on no OS services. Regenerate on Linux with GNU binutils using `as --64 examples/elf-demo.s -o elf-demo.o` and `ld -Ttext=0x1000 -e _start -o elf-demo.elf elf-demo.o`.

## Restrictions
Rejects `ET_DYN`, `PT_INTERP`, `PT_DYNAMIC`, `PT_TLS`, overlapping segments and too many headers. Other non-load program-header types are ignored; unsupported features are not comprehensively detected, so acceptance does not prove a program can run correctly. No relocations, dynamic linking, process startup ABI, guest system calls, TLS, hardware virtual-to-physical translation, guest-driven mapping syscalls or actual executable isolation exist. The stack is not populated with argc/argv or auxiliary vectors. R/W/X bits are application-level only. File loading follows host paths and can race with file changes. Allocation exceptions can propagate. Do not run untrusted files.

## Bounded metadata inspection
`nightfall_cli --inspect-elf FILE` reads the 64-byte ELF header and at most
128 program headers (56 bytes each) from a regular host file. It reports file
size, ELF type, machine, OSABI, entry and program headers, without loading
segments or executing code. It accepts only little-endian ELF64 metadata; it
does not decrypt or process protected formats. A successful inspection is
**not** a compatibility or safety verdict. File paths are not sandboxed and
the file can change between reads.

The separate `--run-elf-sparse` mode loads only original trusted unprotected
test ELFs, with bounded sparse guest memory. It does not implement the PS5
loader or support games. See MEMORY_ARCHITECTURE.md for limits.

## Opt-in ET_DYN research loader

`load_static_pie` is a separate, strictly bounded loader for original,
unprotected x86-64 ELF `ET_DYN` fixtures. It maps non-overlapping PT_LOAD
segments at an explicit aligned load bias, applies selected RELA relocations,
then sets segment permissions and creates a non-overlapping stack. It stages
memory and CPU state, committing only after success. It implements
`R_X86_64_RELATIVE` with symbol zero and `R_X86_64_64` / `R_X86_64_GLOB_DAT`
for ordinary defined symbols or explicitly supplied import bindings. Defined
symbols resolve within the same image without symbol interposition. Bindings
map exact names to guest virtual addresses only; the loader never looks up
host symbols or converts host pointers. Unbound imports return
`unresolved_symbol`; the optional diagnostic reports the first missing name.
The CLI escapes control bytes in names before printing. Every import name, binding list, table and relocation
count is bounded. Bindings do not install executable code, PS5 services or
trampolines; the caller is responsible for their correctness. Guest addresses
may be unmapped and execution will then fault.

`PT_INTERP`, `PT_TLS`, RELRO, executable stacks, unknown program headers,
external library dependencies (`DT_NEEDED`), PLT/JUMP_SLOT, constructors,
versioned symbols, IFUNC, TLS symbols, REL/RELR and unknown dynamic tags are
rejected rather than silently emulated. Hash/debug metadata is ignored.
This is not a dynamic linker or a PS5 ELF/SELF loader, nor a secure sandbox.
It does not parse or decrypt protected files and does not prove compatibility
with any game. As with the original loader, allocation exceptions can propagate.
A caller must provide trusted original input and a suitable guest ABI.

`nightfall_cli --run-linux-static-pie examples/static-pie-reloc.elf 100`
executes the authored no-interpreter fixture under the experimental Linux test
ABI and exits 42. **Do not execute that no-interpreter fixture on the host**:
the host will not apply its relocation. The separately built authored
`pie-native-reloc.elf` includes the normal Linux dynamic interpreter and is
used only as a native comparison. Other authored `.so` fixtures compare
self-defined and explicitly bound guest symbols with Linux `dlopen` in an
isolated test child, not with PS5 software. Sources and reproducible build
commands are in `examples/README.md`.

The deterministic mutation test changes bytes in an authored ELF, checks that
failed loads leave state intact, and checks success invariants. It is not a
coverage-guided fuzzer, independent semantic oracle, or security guarantee.

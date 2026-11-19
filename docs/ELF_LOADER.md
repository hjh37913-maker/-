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

## Opt-in raw SCE-style fixture runner

`nightfall_cli --run-sce-raw FILE [BUDGET]` connects the existing restricted
raw SCE-style relocation loader to the x86-64 interpreter. This is strictly
for trusted, independently authored, unprotected fixtures. File input is bounded
to 8 MiB, guest virtual space to 256 MiB, sparse allocation to 4096 pages, and
execution to at most one million instruction steps. A 64 KiB stack and fixed
0x100000 load bias are used. A normal CPU stop or loader error is reported;
missing symbol/dependency names are escaped before terminal output.

The basic `--run-sce-raw` command supplies **no** import bindings, libraries,
syscall ABI, guest services, firmware, SELF support, decryption, PS5 graphics
or game support.
Dependencies and unresolved imports are rejected; syscalls trap. Successful
loading or HLT does not demonstrate platform or game compatibility. File paths
are not isolated, file reads may race, and this interpreter is not a sandbox;
do not run untrusted files. This command has been compiled on Linux and
cross-compiled for Windows x64, but has not been executed or tested.

### Explicit synthetic service bindings (unverified)

`--run-sce-raw-services FILE BUDGET KIND=SYMBOL [...]` is a separate opt-in
variant for trusted authored fixtures. It supplies only the specified exact
symbol names, binding each to a guest trap for an existing bounded SysV
primitive: `copy`, `move`, `fill`, `compare`, or `strlen`. Quote symbol names
containing shell-special characters. At most 32 service or dependency entries are accepted; duplicate service names
or duplicate dependency IDs of the same kind fail. Services and addresses are
caller assertions, not automatic PS5 NID resolution or PS5 library
implementations. They provide no syscalls, file I/O, graphics or process
startup ABI. Dependency metadata may be asserted explicitly as
`module=ID:MAJOR:MINOR:NAME` or `library=ID:MAJOR:NAME` with decimal IDs and
versions. Such assertions only match metadata; they do **not** load libraries
or implement their functions. Unmatched imports and unprovided module/library
identities still fail. The trap range is below the
image bias and checked for overlap. No external host function is bound.

The raw SCE-style loader additionally implements generic x86-64 relocation
kinds NONE, PC32, PLT32, 32 and 32S for its existing restricted format, with
32-bit extension checks, writable-target requirements and staged rollback.
These new relocation kinds and the service CLI are **not tested**, and their
behavior or use in actual PS5 executables is **not established**. They do not
provide a working PS5 dynamic linker. The loader still rejects text relocations,
TLS, unknown tags and other unsupported features. Do not run untrusted files.

## Experimental multi-image raw loader (not tested)

The C++ API now offers `append_sce_raw_module` to map one provider into an
existing unmapped/sparse guest machine without changing CPU state, optionally
returning owned copies of ordinary visible defined symbol names and guest
addresses. `append_and_start_sce_raw_module` then maps a consumer and stack and
sets its entry/CPU state. Each call stages memory and commits only on success;
a failed later call does not undo earlier successful calls. Caller-provided
bindings and dependencies are assertions, not loaded host libraries.

`load_sce_raw_bundle` provides an all-or-nothing sequence of up to 32 images,
with caller-specified addresses, symbols and identity assertions. The final
image becomes the process entry point. `load_sce_raw_bundle_autobind` is an
opt-in variant that binds exact names from earlier providers; providers must
precede consumers. Duplicate names, including weak/strong combinations and
conflicts with explicit bindings, fail rather than guessing; forward/circular
references, symbol versioning and interposition are not implemented. The
caller must provide a suitable initially unmapped guest memory and account for
the expense of staging copies. Allocation exceptions may propagate.

`--run-sce-bundle BUDGET PROVIDER... EXECUTABLE` exposes the opt-in autobinder
for trusted, independently authored files only. Inputs are limited to 8 MiB
per file, 32 MiB total, 32 images, 4096 sparse pages and 1,000,000 CPU steps.
Biases are fixed at `0x100000 + index * 0x400000`; overlaps or incompatible
alignment fail. No dependency assertions, system services, host functions,
syscalls, constructors, TLS or PS5 ABI are supplied by this CLI. Images with
unfulfilled dependencies are rejected. It is not a sandbox and must not be used
with untrusted data.

**None of these new APIs or the CLI has been executed or tested.** Generic
exact-name linking here is not established PS5 dynamic-linker behavior: SCE
module identities, NIDs, versions, symbol resolution and initialization are
not fully implemented or verified. SELF, encryption, PS5 graphics and games
remain unsupported. Compilation alone is not correctness or compatibility
evidence.

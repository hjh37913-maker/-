# NIGHTFALL — 30-minute work block

**2026-11-19, 13:12:42–13:43:13 UTC.** This is an engineering and
validation block, not a claim of PS5 compatibility.

## Implemented
- CPU: SHL/SHR/SAR, INC/DEC, NOT/NEG, and selected 16-bit MOV forms; includes
  register/memory addressing, access faults, and original freestanding ELF
  fixtures. Flags and instruction restrictions are documented in
  `docs/CPU_ARCHITECTURE.md`.
- Memory and ELF: bounded sparse 4 KiB guest pages with byte permissions,
  transactional mapping, explicit resource-limit errors, and ELF test loader
  integration. `--run-elf-sparse` runs a synthetic test program at virtual
  address `0x100000000`; this is not a PS5 memory map.
- ELF metadata inspector: bounded header-only `--inspect-elf` with no guest
  execution or decryption. It is not a compatibility verdict or sandbox.
- Graphics: CPU-only triangle rasterization, clipping and a 16-bit affine
  depth-buffer experiment with original PPM demos. Neither is a PS5 GPU.

## Validation
Final full Linux x86-64 GCC 12.2 checks: **90/90 CTest tests** in each of
Debug, Release and ASan+UBSan. Portable sparse-vs-flat tests and software
triangle/depth reference comparisons passed. An original sparse high-address
ELF test halted with RAX=42. Windows x64 CLI and portable tests cross-compiled,
but Windows runtime was not available. Android and hardware GPU were not
validated in this block. Allocation failure injection was not performed.

A separate long-running randomized shift check used the frozen Release test
binary SHA-256 `2059ab5060f8516e958194138f20329aa117909abea51764245b20d32ba7fee1`
from 13:16:09 to 13:42:42 UTC, 1,592.861 seconds. It completed successfully;
`shift-results.json` records seeds and counts. The oracle compares results and
defined flags with equivalent native **register shifts**; it does not execute
identical immediate encodings or guest addressing. The executable was built
before subsequent sparse-memory changes, so it is not a full soak test of the
final source tree. An earlier run was stopped to freeze the binary before
continued builds; its results are not included. The very large number of
comparisons is not a measure of game readiness.

During implementation, an old indirect-branch test expected INC/DEC to be
unsupported and was updated to check still-unsupported group extensions. A
sparse test initially had a preprocessor macro comma error; it was corrected.
All regressions passed afterwards. Windows x64 compilation succeeded but does
not establish Windows execution.

## Remaining blockers
PS5 executable loading, process ABI, syscalls, GPU commands, shaders, hardware
rendering, audio, input integration, legal game loading and security isolation
are not implemented. GTA VI and PS5 games do not run. No realistic completion
date or game performance figure can be inferred from this block.

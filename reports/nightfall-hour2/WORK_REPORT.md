# Engineering block: 2026-11-19, starting 16:35:01 UTC

## Scope and truthfulness

This block advances generic, original x86-64/Linux test machinery. It does not
implement the PS5 executable loader, OS ABI, graphics/GPU/shader stack, DRM,
or GTA VI support. There has been no execution of GTA VI or PlayStation game
software, no validation on Windows, and no prediction of a completion date.
The interpreter and Linux test runtime are not security boundaries; only
trusted self-authored unprotected fixtures were run.

## Implementation

* Immutable bounded synthetic files and virtual file descriptors: open,
  openat, close, read/readv, pread64 and lseek. No guest path or descriptor is
  passed to a host syscall. Up to 8 files, 64 KiB each / 256 KiB total, and 19
  descriptor slots. Explicit fixture mode exposes only one authored test file.
* Synthetic Linux x86-64 stat/lstat/fstat/newfstatat metadata; exact supported
  subset and synthetic fields are documented in `docs/LINUX_TEST_ABI.md`.
  `dup`, `dup2`, `dup3` share an open-file offset; independent opens do not.
  Limited `fcntl` tracks descriptor flags and duplicate allocation. There is
  no exec, host filesystem access from guest calls, or full filesystem model.
* CPU BSF/BSR and POPCNT selected 16/32/64-bit register/memory forms. BSF/BSR
  zero-source destination and undefined flags use documented model choices;
  no CPUID/PS5 feature model is claimed.
* Two authored file ELFs, including a freestanding compiled C fixture with a
  normal-alignment entry shim, are run in the test ABI; native execution uses
  separately created private temporary files. The test-only runner and
  deterministic driver cover varied binary data and expected failure paths.

## Regression and differential checks

* Final `./scripts/verify.sh`: Linux Debug, Release, ASan+UBSan each **140/140**.
  Separate clean Release: **140/140**. The clean build includes the same
  authored tests and is not independent software compatibility validation.
* Windows x64 Zig cross-compiled the CLI, portable file/stat/dup/fcntl and CPU
  bit-scan/POPCNT test binaries and file-fixture test runner; not run on Windows.
* Regular native Linux checks include 25,600 stateful read/pread/lseek/readv
  comparisons and 256 selected fstat size/type checks; 32,768 stateful
  open/dup/dup2/close/read/seek comparisons; 32,768 fcntl/dup3/close
  comparisons. Native oracles use only private temporary/memfd data and mapped
  host descriptor numbers, not guest host paths. Metadata not compared to host
  remains synthetic. 196,608 native BSF/BSR register results/ZF comparisons and
  98,304 native POPCNT result/status-flag comparisons passed on this CPU.
* 160,000 varied-input compiled-file fixture executions (17 deterministic
  seeds, including 115,000 using the final clean Release runner) matched independent Python expectations and native temporary-file
  execution. Size range 0–96 bytes includes error cases for lengths over 64.
* Frozen native BSF/BSR soak: 44,322 seeded batches, 8,714,059,776
  comparisons in 1,650.012 seconds. The oracle checks defined results and ZF
  against native register instructions; it does not validate guest memory
  addressing or undefined results/flags. Frozen executable SHA-256:
  `e95626974ea2c04b3871a7e79d4b7e92086948ef481a19f747031de2cc73610f`.
  Seeds were `0x281fa733acc51619 XOR ((batch * 0x9e3779b97f4a7c15) mod 2^64)`.
  Result record: `bit-scan-results.json`.

* Compiled-file soak: 60,000 additional generated guest runs, each compared
  with Python and a native private temporary-file run, across 12 seeded
  batches in 1,101.609 seconds. The frozen runner predates the later fcntl
  addition; it tests the prior synthetic read/stat/file implementation.
  Identities and seeds: `file-fixture-soak.json`.
* Shared-descriptor soak: 708,444,160 seeded stateful comparisons in
  1,050.022 seconds using a frozen binary predating the later fcntl change.
  Operations include successful and error cases, not just successful reads.
  Linux native oracle uses private memfds and mapped descriptors.
  Identities and seeds: `dup-soak.json`.

* Descriptor-flag soak: 3,304,521,728 stateful fcntl/dup3/close
  comparisons across 100,846 seeded batches in 1,000.003 seconds. These
  include invalid-descriptor and other error cases, often with no mutation;
  the count is not a count of complete applications or games.
  The oracle uses native Linux private memfds and mapped descriptors.
  See `fcntl-soak.json` for frozen executable identity and seeds.

## Remaining gaps

The Linux test ABI is partial and differs at error/resource boundaries; it
lacks a dynamic linker, TLS, general filesystem, networking, scheduler,
real signals and an executable sandbox. The x86-64 interpreter lacks many
instructions and architectural facilities and is slow. Cross-compilation is
not evidence of Windows runtime behavior. PlayStation executable formats,
OS compatibility, graphics, game-specific behavior and lawful access to
original game software remain major separate problems. There is no measured
percentage of completion toward GTA VI.

The work block began at 16:35:01 UTC and continued through 17:35:07 UTC,
with report, commit, push and archive finalized afterwards. The first short
bit-scan soak was intentionally stopped to add varying seeds; its partial
results are excluded. No running game or PS5 compatibility is implied by the
component-level results.

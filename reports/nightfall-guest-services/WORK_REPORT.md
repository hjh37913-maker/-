# Synthetic guest services and loader integration — 2026-11-19

## Scope and evidence

This increment adds an explicit service table for five bounded, synthetic SysV
x86-64 primitives (copy, move, fill, compare and string length), and a
budgeted CPU/service dispatcher. Caller-selected guest addresses are trap
identities, never host pointers. An original SCE-style synthetic ELF fixture
is relocated with an explicit binding and calls the length service at three
load biases. This is not a real PS5 system library, PS5 ABI implementation,
production file loader, compatibility layer or security sandbox.

- `./scripts/verify.sh`: Linux x86-64 GCC Debug, Release and ASan+UBSan each
  passed 155/155 tests. After adding further memory-fault assertions, the three
  service tests were rebuilt and passed again in all three configurations.
- `guest_services_libc`: 40,960 authored inputs compared to Linux host libc;
  this validates only the selected operations and their specified result signs.
- Windows x64 service unit test cross-compiled with Zig; not run on Windows.
- The previously implemented CPU rasterizer remains covered by the regression
  suite. It is not a GPU, PS5 shader translator or verified game graphics.
  SwiftShader software Vulkan was attempted but its build was stopped before
  completion; no Vulkan or GPU execution was tested. Independent SPIRV-Tools
  compiled but was not used to validate a project shader.

## Contract and limitations

Callers must configure the registry before taking bindings, keep it alive and
not modify it while the returned string views are in use. Services use the
SysV registers and a guest return address on the guest stack. Up to 128 entries;
operations are capped at 65536 bytes (strlen scans for a terminator at or
before that limit). Overlapping copy is rejected; move snapshots input.
Memory faults and size limits are reported to the caller. A destination may
modify the guest return slot, and the dispatcher reads the resulting value.
Each service call consumes one dispatch budget step; bounded byte operations
still take work proportional to their length.

No authentic PS5 executable, firmware, PS5 hardware, GPU, system library or
GTA VI has been tested. No protected file support or decryption is included.

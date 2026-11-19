# Standalone synthetic loader diagnostic — 2026-11-19

`nightfall_sce_service_demo` executes only an embedded, original synthetic
SCE-style ELF fixture. It maps and relocates it, binds a synthetic strlen-like
service to a guest trap address and runs the guest until HLT at three load
biases. It verifies length 5, five execution steps and stack restoration.
No external game or system files are accepted by this diagnostic.

Reproduce on Linux with CMake and a C++20 compiler:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target nightfall_sce_service_demo
./build/nightfall_sce_service_demo
ctest --test-dir build -R '^sce_service_demo$' --output-on-failure
```

`./scripts/verify.sh`: 156/156 CTest cases passed on Linux x86-64 in each of
Debug, Release and ASan+UBSan. The demo was run natively on Linux. A Windows
x64 executable was cross-compiled with Zig; it has **not** been executed or
validated on Windows. The executable is unsigned and uses the Windows Universal
CRT. The package's BAT file is only a convenience wrapper and cannot provide
compatibility evidence. Run only if you trust this unsigned experimental build.

This is a diagnostic for authored test code, not a PS5 file loader validated
on real PS5 inputs, a system library, GPU, game emulator or GTA VI launcher.
No copyrighted game data is supplied. The software is not a security sandbox.

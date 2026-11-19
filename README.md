# NIGHTFALL

An early-stage, research-only PS5 emulation project. **No PS5 binaries, games, or GTA VI can run.** The CLI runs a hard-coded synthetic x86-64 sequence by default and can also run bounded original raw-byte test programs; it is not a game launcher. See [project state](docs/PROJECT_STATE.md) and [limitations](docs/KNOWN_LIMITATIONS.md).

## Build and run
Install CMake 3.20+ and a C++20 compiler (on Windows: Visual Studio 2022 with Desktop development with C++, plus CMake). From a developer command prompt:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```
Run `build/Debug/nightfall_cli.exe` on Windows with a multi-config generator, or `./build/nightfall_cli` on a single-config Unix generator. Windows has not been tested. Release: use `-DCMAKE_BUILD_TYPE=Release` for single-config generators and `--config Release` for multi-config generators. Details in [BUILD](docs/BUILD.md).

On Linux, `./scripts/verify.sh` runs the current Debug, Release and sanitizer suites together. A Windows script is provided but untested; see [testing](docs/TESTING.md) and [coverage](docs/COVERAGE.md).

## What exists
A fixed guest byte array with experimental byte-granular mappings and R/W/X access checks, a small subset of x86-64 instruction encodings, and budgeted execution with explicit stop reasons. CTest checks memory boundaries, selected arithmetic flags, faults and a synthetic integration program. On Linux x86-64 with GCC or Clang, native differential tests check ADD, SUB and CMP; see [testing](docs/TESTING.md). An optional microbenchmark is described in [performance](docs/PERFORMANCE.md). A CPU-only software graphics experiment can generate an original PPM: `./build/nightfall_graphics_demo output.ppm` (on Windows multi-config builds use `build/Debug/nightfall_graphics_demo.exe`). This is not PS5 graphics support. An experimental synchronous bounded host-file reader and CRC-32 are also available; see [storage](docs/STORAGE_ARCHITECTURE.md). They do not sandbox paths. An experimental synthetic digital input mapper is described in [input](docs/INPUT_ARCHITECTURE.md); it does not detect physical devices. A logical timer queue is described in [runtime](docs/RUNTIME_ARCHITECTURE.md); it is not a PS5 service. The CLI can load a small raw original test program or a restricted unprotected ELF64 test executable. It does not provide OS services, a PS5 loader, or isolation for untrusted code. Run `./build/nightfall_cli --run-raw examples/register-demo.bin 5` to obtain RAX=14. Run `./build/nightfall_cli --run-elf examples/elf-demo.elf 5` for the original ELF test. See [examples](examples/README.md) and [ELF loader](docs/ELF_LOADER.md).

### Execution diagnostics
For `--run-raw` and `--run-elf`, the CLI reports attempted instruction count and stopping RIP. On invalid instruction or memory fault it also shows up to 15 execute-accessible bytes at the stop RIP. The bytes are not a disassembly; a memory fault may be from a data access rather than instruction fetch, and the data address is not currently reported. `examples/unsupported-demo.bin` demonstrates the failure path. These modes are limited to original unprotected test programs and are not a sandbox or a PS5 game launcher.

# Work block: CPU multiplication and sign extension

- Started 2026-11-19 14:17:35 UTC; work and verification continued at least 15 minutes.
- Implemented F6/F7 one-operand unsigned and signed multiplication, 16/32/64-bit two- and three-operand signed IMUL, and selected CBW/CWDE/CDQE/CWD/CDQ/CQO encodings. Full-width 64-bit multiplication uses portable unsigned 32-bit limbs, not compiler-specific 128-bit integers. Precise limits, prefixes and flags are documented in `docs/CPU_ARCHITECTURE.md`.
- Added an original, native-safe static Linux ELF fixture and compared its stdout and exit with native execution. This is not a game or PS5 binary.
- Final Linux GCC Debug, Release and ASan+UBSan: 106/106 CTest cases passed in each. Selected native comparisons per applicable run: 458,752 full-product MUL/IMUL, including exhaustive byte pairs; 196,608 selected multi-operand IMUL cases; 98,304 accumulator extensions. These compare only specified instructions and inputs, not CPU compatibility as a whole.
- An additional clean Release build in `/tmp/nightfall-clean-multiply` passed 106/106 tests.
- An initial SIB test failed because the test encoded a scale of 2 while expecting a scale of 4. Corrected the fixture encoding, then all three full configurations passed; no tests were removed.
- Windows x64 CLI and portable tests were cross-compiled with Zig, but not executed on Windows. No actual Windows runtime, performance, PS5 software or GTA VI execution was tested. No DRM circumvention or protected files were involved.

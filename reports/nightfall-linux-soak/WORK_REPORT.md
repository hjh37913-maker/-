# Work block: bounded Linux x86-64 test ABI

- Block start: 2026-11-19 13:44:20 UTC; verification continued through at least 14:14:20 UTC.
- Added a syscall trap and intentionally small Linux x86-64 test process model: bounded startup stack and arguments; prebuffered stdin; captured stdout/stderr; read, write, readv, writev, synthetic bounded brk, exit/exit_group and ENOSYS fallback. No guest host syscall passthrough.
- Added six original static ELF fixtures and CLI modes, portable fault and stateful-model tests, plus selected native Linux private-pipe differential tests. The model intentionally differs from Linux in edge behavior and is not a security sandbox.
- Final Linux GCC Debug, Release, and ASan+UBSan: 101/101 CTest tests passed in each configuration. This includes 50,000 stateful operations compared to a separate software model and 8,192 cases per native basic read, readv, write and writev oracle in each applicable run. See test sources for scope.
- Windows x64 CLI and portable tests cross-compiled with Zig. No Windows execution was available; no Windows runtime or performance claim is made.
- A separate frozen Release native-write oracle ran for 1,407.406 seconds, completing 21,182 batches and 1,388,183,552 selected synthetic write comparisons through private pipes without a mismatch. The frozen binary hash, timing and test scope are in `write-results.json`. Repeated generated cases are not full Linux compatibility, game execution or a security proof.
- No PS5 loader, OS ABI, GPU/shader compatibility, protected-content support, or GTA VI execution was implemented or tested. See `docs/LINUX_TEST_ABI.md`.

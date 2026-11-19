# Storage test cases

Procedure: build and run `ctest --test-dir build -R '^storage_tests$' --output-on-failure`. The executable generates its own 16-byte sequential fixture, empty file and synthetic instruction stream in a temporary directory. Environment: C++20 and filesystem-capable local host; actual result: all checks passed on Linux x86-64, GCC 12.2 Debug, Release and ASan/UBSan. Each item below is one assertion and has status TESTED in that environment; it is not a separate CTest executable or a claim of exhaustive coverage.

| IDs | Purpose and expected result |
|---|---|
| 01–08 | Read whole file, first byte, last byte, interior, second half, interior to near end, empty at start, empty at EOF: exact bytes |
| 09–16 | Empty interior, several offset/length pairs, remainder and exact limit: exact bytes or success |
| 17–18 | Exceed zero limit and limit by one: `limit_exceeded` |
| 19–24 | Too-long read, past EOF, zero-length beyond EOF, maximal offset with one and zero length, crossing EOF: `range_error` |
| 25–28 | Missing path, directory, empty file zero-byte read, empty file one-byte read: open error, not regular, success, range error |
| 29–30 | Out-of-range and missing-file errors return empty data |
| 31–37 | CRC known vectors: empty, `123456789`, `abc`, binary 0–15, zero byte, FF byte, `Nightfall` |
| 38–39 | Incremental CRC equals single-pass known vector; reset restores empty CRC |
| 40 | Read original synthetic instruction bytes from disk, write guest memory, execute, halt with RAX 42 |

No read-race, inaccessible permissions, platform-specific filesystem errors, exhaustion, asynchronous performance or Windows tests were performed.

# Experimental Linux x86-64 test ABI

This is an intentionally small, deterministic test environment for original,
static x86-64 ELF programs. It is **not** a PS5 ABI, a general Linux process
implementation, a security boundary, or evidence that games can run. Use only
trusted, self-authored test binaries. Guest system calls are interpreted by
this project; their file descriptors and pointers are never forwarded to the
host OS. The CLI itself reads the requested host ELF and, only in the explicit
stdin mode, reads host standard input.

## Implemented model

* The CPU recognizes `SYSCALL` as a trap; the runtime advances RIP, sets RCX
  to the return address and R11 to RFLAGS. Kernel mode, privilege changes,
  FMASK, signals and threads are not modeled.
* Startup stack has argc, argv, a terminating null, an empty environment, and
  `AT_NULL` only; it is not a complete Linux auxiliary vector. Arguments are
  bounded, and the initial stack pointer is aligned to 16 bytes.
* Syscalls: buffered `read`/`readv` from fd 0, buffered `write`/`writev` to
  fd 1 or 2, a bounded page-mapped synthetic `brk`, and `exit`/`exit_group`.
  Other syscalls return `-ENOSYS`. Counts, memory, I/O, argument strings and
  execution are bounded. In particular the combined output limit is a project
  constraint, and `readv` validates touched ranges before mutation; edge cases
  need not have native Linux's partial-progress semantics. The test heap is
  at most 1 MiB; the input buffer is at most 65536 bytes.
* Guest divide-by-zero and quotient overflow stop with a modeled `divide_error`;
  no SIGFPE or guest signal delivery is implemented.
* No dynamic linker, TLS, process environment, filesystem, networking, thread
  scheduler, PS5 loader, graphics API, GPU/shader execution or DRM handling.
  Loader and CPU instruction support are deliberately incomplete.

## Running original examples

Build according to `docs/BUILD.md`. From the repository root, with the CLI path
adjusted to your build directory:

```sh
build-verify-debug/nightfall_cli --run-linux-elf examples/linux-hello.elf 100
build-verify-debug/nightfall_cli --run-linux-elf examples/linux-args.elf 100 first second
printf 'hello' | build-verify-debug/nightfall_cli --run-linux-elf-stdin examples/linux-echo.elf 100
```

The first argument after the ELF is an optional execution-step budget; repeated
string instructions consume a step per element, including one for a zero count.
Subsequent
arguments become guest argv after a synthetic argv[0]. Without the stdin mode,
guest fd 0 starts empty. The CLI prints captured guest output and a diagnostic
on host stderr, and returns the guest exit status on modeled exit. Interrupted,
unsupported, or faulty programs produce a failure rather than a game launch.
The examples are self-authored static fixtures, not PlayStation software.

## Test scope

`./scripts/verify.sh` runs the full CTest suite in Linux Debug, Release and
ASan+UBSan configurations. Portable tests cover syscall and stack boundaries,
heap, vector I/O, instruction budgets, and 50,000 stateful steps against a
separate bounded software model (not an independent native oracle). On Linux
x86-64, private-pipe comparisons exercise 8,192 generated cases per basic
read, write, readv and writev oracle; original Linux ELF examples are compared
with native output and exit where applicable. These are finite test cases, not
proof of full syscall compatibility. The long-running write soak repeatedly
executes the frozen native oracle; its detailed result is recorded separately.
Windows x64 was cross-compiled, not runtime tested.

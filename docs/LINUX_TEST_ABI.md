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
* Syscalls: buffered `read`/`readv` and `write`/`writev`; `open`, `openat`,
  `close`, `dup`/`dup2`/`dup3`, `lseek`, `pread64` and selected stat calls for opt-in immutable
  in-memory fixtures;
  a bounded page-mapped synthetic `brk`, a constrained anonymous
  `mmap`/`mprotect`/`munmap` arena, and `exit`/`exit_group`.
  Other syscalls return `-ENOSYS`. Counts, memory, I/O, argument strings and
  execution are bounded. In particular the combined output limit is a project
  constraint, and `readv` validates touched ranges before mutation; edge cases
  need not have native Linux's partial-progress semantics. The test heap is
  at most 1 MiB; the input buffer is at most 65536 bytes.
* The anonymous arena is at most 4 MiB, with at most 1 MiB per request.
  Only a zero hint, fd -1, offset zero, exact private-anonymous flags and
  R/W/X bits are supported. First-fit selection and confined `mprotect`/
  `munmap` deliberately differ from Linux; holes are ignored by `munmap`,
  but foreign mappings are never unmapped. The CLI arena is
  0x90000000–0x90400000. No guest mmap reaches the host OS.
* `add_test_file` registers up to eight absolute literal names, each at most
  255 bytes, with up to 65536 bytes each and 262144 total, before execution.
  Files are immutable copies, exact-match names are not host paths, and no
  path traversal or symlink resolution is performed. Open supports read-only
  mode and optional `O_CLOEXEC`, `openat` only supports `AT_FDCWD`, and there
  are 19 modeled descriptor slots (including standard streams). Closing a
  descriptor permits its reuse. Duplicated descriptors share the same modeled
  open-file offset; independent opens have independent offsets. Duplication
  can also redirect the captured standard streams; `O_CLOEXEC` is accepted but
  no exec is modeled. The model's errors, resource limits and
  `readv` atomic fault behavior differ from Linux; there is no file write,
  directory API, host filesystem or process exec. `stat`, `lstat`, `fstat`
  and `newfstatat` use a 144-byte x86-64 Linux layout for registered files,
  with deterministic synthetic inode, mode 0444, sizes and zero timestamps;
  metadata is not host metadata. `newfstatat` only accepts AT_FDCWD and zero
  flags. `fstat` for the standard streams is not implemented.
* Guest divide-by-zero and quotient overflow stop with a modeled `divide_error`;
  no SIGFPE or guest signal delivery is implemented. Unaligned MOVDQA/MOVAPS
  memory operands stop with a modeled `alignment_fault`, not a delivered #GP.
* No dynamic linker, TLS, process environment, host filesystem, networking, thread
  scheduler, PS5 loader, graphics API, GPU/shader execution or DRM handling.
  Loader and CPU instruction support are deliberately incomplete.

## Running original examples

Build according to `docs/BUILD.md`. From the repository root, with the CLI path
adjusted to your build directory:

```sh
build-verify-debug/nightfall_cli --run-linux-elf examples/linux-hello.elf 100
build-verify-debug/nightfall_cli --run-linux-elf examples/linux-args.elf 100 first second
printf 'hello' | build-verify-debug/nightfall_cli --run-linux-elf-stdin examples/linux-echo.elf 100
build-verify-debug/nightfall_cli --run-linux-elf-fixture examples/linux-file.elf 500 /test/input
```

The first argument after the ELF is an optional execution-step budget; repeated
string instructions consume a step per element, including one for a zero count.
Subsequent arguments become guest argv after a synthetic argv[0]. Without the stdin mode,
guest fd 0 starts empty. The fixture mode explicitly registers one authored
in-memory file `/test/input`; regular modes register no files. It never opens
the guest path on the host. The CLI prints captured guest output and a diagnostic
on host stderr, and returns the guest exit status on modeled exit. Interrupted,
unsupported, or faulty programs produce a failure rather than a game launch.
The examples are self-authored static fixtures, not PlayStation software.

## Test scope

`./scripts/verify.sh` runs the full CTest suite in Linux Debug, Release and
ASan+UBSan configurations. Portable tests cover syscall and stack boundaries,
heap, mmap ownership, vector I/O, execution budgets, and 50,000 stateful steps against a
separate bounded software model (not an independent native oracle). On Linux
x86-64, private-pipe comparisons exercise 8,192 generated cases per basic
read, write, readv and writev oracle. A separate native private-memfd test
compares 25,600 stateful file read/pread/lseek/readv operations and 256 file
size/type checks for fstat; inode, permissions and timestamps are synthetic.
A further 32,768 stateful operations compare open/dup/dup2/close/read/seek
against Linux private memfds; test descriptor numbers are mapped to private
host descriptors and do not involve guest paths.
Original
Linux ELF examples are compared
with native output and exit where applicable. These are finite test cases, not
proof of full syscall compatibility. A further 50,000 mmap arena operations
are compared with a separate bounded software model, and 1,024 selected cases
with native Linux private anonymous mappings; neither covers real game ABIs.
An additional compiled C fixture combines anonymous allocation, input,
read-only protection, hashing, output and unmapping.
The long-running write soak repeatedly
executes the frozen native oracle; its detailed result is recorded separately.
Windows x64 was cross-compiled, not runtime tested.

The authored `linux-file-c.elf` additionally runs with generated fixture data
via the test-only `linux_file_fixture_runner`. The Python stress driver checks
the 0–96-byte inputs and exit statuses against a Python reference and, on
Linux x86-64, the same original ELF reading a private temporary host file.
Guest file paths remain virtual. Reproduce with
`python3 scripts/stress_linux_files.py --runner build-verify-release/linux_file_fixture_runner --cases 5000`.
These finite comparisons do not imply arbitrary program, OS or game support.

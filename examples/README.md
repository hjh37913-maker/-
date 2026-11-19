# Original raw test program

`register-demo.bin` is an independently authored 20-byte instruction sequence, not a game or executable file format:

```text
49 b8 07 00 00 00 00 00 00 00   mov r8, 7
4c 89 c1                        mov rcx, r8
4c 01 c1                        add rcx, r8
48 89 c8                        mov rax, rcx
f4                              synthetic halt sentinel
```

After building, run `./build/nightfall_cli --run-raw examples/register-demo.bin 5` (for a Windows multi-config build: `build/Debug/nightfall_cli.exe --run-raw examples/register-demo.bin 5`). Expected: halted, RAX=14, RIP=20. Raw means bytes only, not an ELF/PE/PS5 loader. Only run original test files: this interpreter is not a security sandbox and does not support protected content. Guest instruction budget defaults to 10,000 and must be between 1 and 1,000,000. Input is limited to 65,536 bytes, loaded at address zero; file-backed bytes become read/execute and the remainder is readable and writable, not executable. No PS5 binary is supported.

## Memory test
`memory-demo.bin` is an independently authored 27-byte sequence: `mov rbx, 128`, `mov r8, 42`, `mov [rbx], r8`, `mov rax, [rbx]`, synthetic HLT. Run `./build/nightfall_cli --run-raw examples/memory-demo.bin 5`; expected RAX=42, RIP=27. The guest data area is intentionally writable; no host sandboxing is implied.

## Stack and call test
`stack-demo.bin` is an original sequence: MOV RAX,40; CALL rel32; HLT; ADD RAX,2; RET. The raw test CLI initializes RSP to 65,536. Run `./build/nightfall_cli --run-raw examples/stack-demo.bin 5`; expected halted with RAX=42. Stack data is in the writable non-executable portion of the synthetic address space when the input does not occupy it.

## ELF test
`elf-demo.s` is original assembly; `elf-demo.elf` is the corresponding x86-64 GNU-binutils-built ET_EXEC fixture. Run `./build/nightfall_cli --run-elf examples/elf-demo.elf 5` to execute the independent MOV/CALL/ADD/RET/HLT sequence, expected RAX=42. The loader is intentionally restricted; see ../docs/ELF_LOADER.md.

## Freestanding C fixture
`compiler-demo.c` is an original freestanding C program. `compiler-demo.elf` was built on Linux with GCC 12.2 and GNU ld using:

```sh
gcc -O1 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -nostdlib -no-pie -Wl,-Ttext=0x1000 -Wl,-e,_start -Wl,--build-id=none -o compiler-demo.elf compiler-demo.c
./build/nightfall_cli --run-elf examples/compiler-demo.elf 20
```

The C compiler and linker versions or flags may generate different instructions and a different binary. This example uses the interpreter's synthetic HLT sentinel: **do not execute the ELF directly on the host**. Expected interpreter result is RAX=42. No libc, syscalls or guest ABI are involved.

## Freestanding C loop
`loop-demo.c` and its original GCC 12.2 `loop-demo.elf` exercise a function call, register TEST, conditional branches and a loop. Build with the same GCC flags as the previous C fixture, changing the input and output names. Run `./build/nightfall_cli --run-elf examples/loop-demo.elf 100`; expected RAX=45. The ELF uses synthetic HLT; **do not execute it directly on the host**. No libc, syscalls or PS5 services are involved.

## 32-bit C loop
`int-loop-demo.c` and `int-loop-demo.elf` are original GCC 12.2 freestanding examples using 32-bit unsigned arithmetic. Build with the same flags as the other C example, changing input and output names. Run `./build/nightfall_cli --run-elf examples/int-loop-demo.elf 100`; expected RAX=45. This file uses the synthetic HLT sentinel: **do not execute it directly on the host**. No operating-system or PS5 interfaces are exercised.

## 32-bit array fixture
`array-demo.c` and `array-demo.elf` are an original freestanding C example compiled using GCC 12.2 and the same flags as above. The function uses local 32-bit values and returns 42. Run `./build/nightfall_cli --run-elf examples/array-demo.elf 100`. The ELF uses the synthetic HLT sentinel: **do not execute it directly on the host**. No OS or PS5 interface is exercised.

## Memory arithmetic fixture
`memory-arithmetic-demo.c` uses explicit inline x86 assembly for 32-bit memory ADD/SUB/CMP, compiled with GCC 12.2 using the same freestanding flags as the array fixture. Run `./build/nightfall_cli --run-elf examples/memory-arithmetic-demo.elf 100` and expect RAX=42 at synthetic HLT. Do not execute the ELF directly on the host. It does not exercise any OS or console interface.

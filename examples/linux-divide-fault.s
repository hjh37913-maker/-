# Original x86-64 Linux fault fixture: intentional divide by zero.
# Only run under the test runtime, or in an isolated child with SIGFPE expected
# and core dumps disabled; it must never be treated as a successful program.
.global _start
.section .text
_start:
    mov $1, %eax
    xor %edx, %edx
    xor %ebx, %ebx
    div %rbx
    mov $60, %eax
    xor %edi, %edi
    syscall

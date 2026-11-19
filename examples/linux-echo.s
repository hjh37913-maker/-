# Original Linux x86-64 fixture. Reads at most 32 bytes, echoes to stdout, exits.
# This fixture is safe to run natively on Linux with controlled stdin.
.global _start
.section .text
_start:
    xor %eax, %eax
    xor %edi, %edi
    lea buffer(%rip), %rsi
    mov $32, %edx
    syscall
    mov %eax, %edx
    mov $1, %eax
    mov $1, %edi
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall
.section .bss
.lcomm buffer, 32

# Original native-safe Linux x86-64 program using writev and exit.
.global _start
.section .text
_start:
    mov $20, %eax
    mov $1, %edi
    lea vectors(%rip), %rsi
    mov $3, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall
.section .rodata
first:  .ascii "Hello"
second: .ascii ", "
third:  .ascii "vector!\n"
.section .data
.balign 8
vectors:
    .quad first, 5
    .quad second, 2
    .quad third, 8

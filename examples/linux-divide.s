# Original native-safe static Linux x86-64 test of signed and unsigned division.
.global _start
.section .text
_start:
    mov $-17, %rax
    cqo
    mov $5, %rbx
    idiv %rbx
    cmp $-3, %rax
    jne fail
    cmp $-2, %rdx
    jne fail
    xor %eax, %eax
    mov $1, %edx
    mov $3, %ebx
    div %rbx
    movabs $0x5555555555555555, %rcx
    cmp %rcx, %rax
    jne fail
    cmp $1, %rdx
    jne fail
    mov $1, %eax
    mov $1, %edi
    lea message(%rip), %rsi
    mov $10, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall
fail:
    mov $60, %eax
    mov $1, %edi
    syscall
.section .rodata
message: .ascii "Divide OK\n"

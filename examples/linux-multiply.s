# Original x86-64 Linux test: signed/unsigned multiply and sign extension.
.global _start
.section .text
_start:
    mov $-7, %rax
    mov $6, %rbx
    imul %rbx, %rax
    cmp $-42, %rax
    jne fail
    imul $-3, %rbx, %rax
    cmp $-18, %rax
    jne fail
    mov $-1, %rax
    mov $2, %rbx
    mul %rbx
    cmp $-2, %rax
    jne fail
    cmp $1, %rdx
    jne fail
    mov $-1, %rax
    cqo
    cmp $-1, %rdx
    jne fail
    mov $1, %eax
    mov $1, %edi
    lea message(%rip), %rsi
    mov $12, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall
fail:
    mov $60, %eax
    mov $1, %edi
    syscall
.section .rodata
message: .ascii "Multiply OK\n"

# Original freestanding Linux x86-64 multi-limb arithmetic test.
.global _start
.section .text
_start:
    mov $-1, %rax
    mov $4, %rdx
    mov $1, %rbx
    xor %ecx, %ecx
    add %rbx, %rax
    adc %rcx, %rdx
    cmp $0, %rax
    jne fail
    cmp $5, %rdx
    jne fail
    sub %rbx, %rax
    sbb %rcx, %rdx
    cmp $-1, %rax
    jne fail
    cmp $4, %rdx
    jne fail
    mov $1, %eax
    mov $1, %edi
    lea message(%rip), %rsi
    mov $9, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall
fail:
    mov $60, %eax
    mov $1, %edi
    syscall
.section .rodata
message: .ascii "Carry OK\n"

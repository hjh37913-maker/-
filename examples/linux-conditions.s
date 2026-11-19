# Original Linux x86-64 test of conditional moves and byte condition results.
.global _start
.section .text
_start:
    mov $5, %rax
    mov $7, %rbx
    cmp %rbx, %rax
    cmovl %rbx, %rax
    cmp $7, %rax
    setz %cl
    movzbl %cl, %ecx
    cmp $1, %ecx
    jne fail
    mov $1, %eax
    mov $1, %edi
    lea message(%rip), %rsi
    mov $14, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall
fail:
    mov $60, %eax
    mov $1, %edi
    syscall
.section .rodata
message: .ascii "Conditions OK\n"

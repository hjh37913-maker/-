# Original native-safe Linux x86-64 fixture: allocate 16 bytes, write "OK\n".
.global _start
.section .text
_start:
    mov $12, %eax
    xor %edi, %edi
    syscall
    mov %rax, %rdi
    add $16, %rdi
    mov %rdi, %rbx
    mov $12, %eax
    syscall
    cmp %rbx, %rax
    jne fail
    movb $'O', -16(%rax)
    movb $'K', -15(%rax)
    movb $10, -14(%rax)
    lea -16(%rax), %rsi
    mov $1, %eax
    mov $1, %edi
    mov $3, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall
fail:
    mov $60, %eax
    mov $11, %edi
    syscall

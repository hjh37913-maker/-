# Original static Linux fixture. argv[1] is a path: in Nightfall it is an
# opt-in in-memory test file; native integration uses a temporary local file.
.global _start
.section .text
_start:
    mov (%rsp), %rax
    cmp $2, %rax
    jb fail
    mov $257, %eax
    mov $-100, %rdi
    mov 16(%rsp), %rsi
    xor %edx, %edx
    xor %r10d, %r10d
    syscall
    test %rax, %rax
    js fail
    mov %rax, %r12
    mov $5, %eax
    mov %r12, %rdi
    lea statbuf(%rip), %rsi
    syscall
    test %rax, %rax
    jne fail
    cmpq $26, statbuf+48(%rip)
    jne fail
    mov $17, %eax
    mov %r12, %rdi
    lea buffer(%rip), %rsi
    mov $9, %edx
    xor %r10d, %r10d
    syscall
    cmp $9, %rax
    jne fail
    mov $8, %eax
    mov %r12, %rdi
    mov $10, %esi
    xor %edx, %edx
    syscall
    cmp $10, %rax
    jne fail
    mov $0, %eax
    mov %r12, %rdi
    lea buffer+9(%rip), %rsi
    mov $9, %edx
    syscall
    cmp $9, %rax
    jne fail
    mov $3, %eax
    mov %r12, %rdi
    syscall
    test %rax, %rax
    jne fail
    mov $1, %eax
    mov $1, %edi
    lea buffer(%rip), %rsi
    mov $18, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall
fail:
    mov $60, %eax
    mov $1, %edi
    syscall
.section .bss
.lcomm buffer, 18
.lcomm statbuf, 144

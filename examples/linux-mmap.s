# Original native-safe Linux x86-64 anonymous mmap/mprotect/munmap fixture.
.global _start
.section .text
_start:
    mov $9, %eax
    xor %edi, %edi
    mov $4096, %esi
    mov $3, %edx
    mov $34, %r10d
    mov $-1, %r8
    xor %r9d, %r9d
    syscall
    test %rax, %rax
    js fail
    mov %rax, %r12
    movb $77, (%r12)
    mov $10, %eax
    mov %r12, %rdi
    mov $4096, %esi
    mov $1, %edx
    syscall
    test %rax, %rax
    jne fail
    movzbl (%r12), %ecx
    cmp $77, %ecx
    jne fail
    mov $1, %eax
    mov $1, %edi
    mov %r12, %rsi
    mov $1, %edx
    syscall
    mov $11, %eax
    mov %r12, %rdi
    mov $4096, %esi
    syscall
    test %rax, %rax
    jne fail
    mov $60, %eax
    xor %edi, %edi
    syscall
fail:
    mov $60, %eax
    mov $1, %edi
    syscall

# Entry shim for original freestanding C test, normal SysV x86-64 call alignment.
.global _start
.extern guest_main
.section .text
_start:
    mov (%rsp), %rdi
    lea 8(%rsp), %rsi
    and $-16, %rsp
    call guest_main
    mov %eax, %edi
    mov $60, %eax
    syscall
.section .note.GNU-stack,"",@progbits

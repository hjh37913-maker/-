# Original unprotected ET_DYN fixture. Native test uses dlopen to call compute.
.global _start, compute, internal_object
.section .text
_start:
    call local_compute
    mov %eax, %edi
    mov $60, %eax
    syscall
compute:
local_compute:
    mov internal_object@GOTPCREL(%rip), %rax
    mov (%rax), %eax
    mov pointer(%rip), %rdx
    add (%rdx), %eax
    ret
.section .data
internal_object: .quad 21
pointer: .quad internal_object
.section .note.GNU-stack,"",@progbits

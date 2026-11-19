# Original Linux test program: the loader must relocate ptr before entry.
.global _start
.section .text
_start:
    mov ptr(%rip), %rax
    mov (%rax), %edi
    mov $60, %eax
    syscall
.section .data
value: .quad 42
ptr: .quad value
.section .note.GNU-stack,"",@progbits

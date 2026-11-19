# Original Linux x86-64 test of bounded REP copy and search.
.global _start
.section .text
_start:
    cld
    lea message(%rip), %rsi
    lea buffer(%rip), %rdi
    mov $10, %ecx
    rep movsb
    lea buffer(%rip), %rdi
    mov $10, %ecx
    mov $79, %eax
    repne scasb
    jne fail
    cmp $2, %rcx
    jne fail
    mov $1, %eax
    mov $1, %edi
    lea buffer(%rip), %rsi
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
message: .ascii "String OK\n"
.section .bss
.lcomm buffer, 10

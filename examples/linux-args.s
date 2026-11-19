# Original standalone Linux x86-64 fixture. Prints five bytes of argv[1]
# if exactly one argument is supplied; exits 7. Native-safe for "hello".
.global _start
.section .text
_start:
    cmpq $2, (%rsp)
    jne fail
    mov 16(%rsp), %rsi
    mov $1, %eax
    mov $1, %edi
    mov $5, %edx
    syscall
    mov $60, %eax
    mov $7, %edi
    syscall
fail:
    mov $60, %eax
    mov $9, %edi
    syscall

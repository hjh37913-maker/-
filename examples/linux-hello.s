# Original standalone Linux x86-64 fixture. Only write(1) and exit(60).
# Unlike the synthetic HLT examples, this program can safely run on Linux.
.global _start
.section .text
_start:
    mov $1, %eax
    mov $1, %edi
    lea message(%rip), %rsi
    mov $message_end-message, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall
.section .rodata
message:
    .ascii "Hello from an original test program!\n"
message_end:

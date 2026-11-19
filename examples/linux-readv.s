# Original native-safe Linux x86-64 test: scatter 5 input bytes and gather them.
.global _start
.section .text
_start:
    mov $19, %eax
    xor %edi, %edi
    lea vectors(%rip), %rsi
    mov $2, %edx
    syscall
    mov $20, %eax
    mov $1, %edi
    lea vectors(%rip), %rsi
    mov $2, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall
.section .data
.balign 8
vectors:
    .quad buffer0, 2
    .quad buffer1, 3
.section .bss
.lcomm buffer0, 2
.lcomm buffer1, 3

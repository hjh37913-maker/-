.global _start, compute, guest_stub
.text
_start:
 call local_compute
 mov %eax,%edi
 mov $60,%eax
 syscall
compute:
local_compute:
 mov host_export@GOTPCREL(%rip),%rax
 call *%rax
 ret
guest_stub:
 mov $42,%eax
 ret
.section .note.GNU-stack,"",@progbits

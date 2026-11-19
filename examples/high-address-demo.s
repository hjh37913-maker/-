# Original freestanding test for sparse guest address space.
# Synthetic HLT is an interpreter sentinel: DO NOT run on the host.
.global _start
_start:
    mov $40, %rax
    add $2, %rax
    hlt

# Independently authored native Linux oracle, not a guest system library.
.global host_export
.text
host_export:
    mov $42, %eax
    ret
.section .note.GNU-stack,"",@progbits

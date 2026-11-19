.global _start
.section .text
_start:
  movabs $40, %rax
  call add_two
  .byte 0xf4
add_two:
  .byte 0x48, 0x05, 0x02, 0x00, 0x00, 0x00
  ret

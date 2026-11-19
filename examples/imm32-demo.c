/* Original freestanding interpreter fixture. HLT is a synthetic stop sentinel. */
__attribute__((noreturn)) void _start(void) {
    unsigned long long value=42;
    __asm__ volatile("addq $268435456, %0; subq $268435456, %0; cmpq $42, %0" : "+m" (value) : : "cc");
    __asm__ volatile("movq %0, %%rax; hlt" : : "m" (value) : "rax", "memory");
    __builtin_unreachable();
}

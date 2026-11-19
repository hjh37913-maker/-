/* Original freestanding test fixture; HLT is a synthetic interpreter sentinel. */
__attribute__((noreturn)) void _start(void) {
    unsigned long long value = 10;
    __asm__ volatile("addq $35, %0; subq $3, %0; cmpq $42, %0" : "+m" (value) : : "cc");
    __asm__ volatile("movq %0, %%rax; hlt" : : "m" (value) : "rax", "memory");
    __builtin_unreachable();
}

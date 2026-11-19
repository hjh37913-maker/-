/* Original freestanding fixture. HLT is an interpreter stop sentinel. */
__attribute__((noreturn)) void _start(void) {
    unsigned value = 10;
    __asm__ volatile("addl $35, %0; subl $3, %0; cmpl $42, %0" : "+m" (value) : : "cc");
    __asm__ volatile("movl %0, %%eax; hlt" : : "m" (value) : "eax", "memory");
    __builtin_unreachable();
}

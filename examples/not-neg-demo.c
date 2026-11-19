/* Original freestanding test: HLT is a synthetic interpreter sentinel. */
__attribute__((noinline)) unsigned long change(unsigned long value) {
    __asm__ volatile("notq %0\n\tnegq %0" : "+r" (value) : : "cc");
    return value;
}
__attribute__((noreturn)) void _start(void) {
    unsigned long result=change(41);
    __asm__ volatile("" : : "a" (result));
    __asm__ volatile("hlt");
    __builtin_unreachable();
}

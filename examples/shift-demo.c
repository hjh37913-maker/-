/* Original freestanding test; its HLT is an interpreter sentinel. */
__attribute__((noinline)) unsigned long combine(unsigned long x, unsigned count) {
    return ((x << count) | (x >> count)) + 22;
}
__attribute__((noreturn)) void _start(void) {
    unsigned long result=combine(8, 1);
    __asm__ volatile("" : : "a" (result));
    __asm__ volatile("hlt");
    __builtin_unreachable();
}

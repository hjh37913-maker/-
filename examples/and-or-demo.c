/* Original freestanding test; HLT is an interpreter stop sentinel, not a host program. */
__attribute__((noinline)) unsigned combine(unsigned a, unsigned b, unsigned mask) {
    return (a | b) & mask;
}
__attribute__((noreturn)) void _start(void) {
    unsigned result = combine(40, 10, 42);
    __asm__ volatile("" : : "a" (result));
    __asm__ volatile("hlt");
    __builtin_unreachable();
}

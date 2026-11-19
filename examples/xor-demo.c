/* Original freestanding C fixture; HLT is an interpreter stop sentinel. */
__attribute__((noinline)) unsigned combine(unsigned a, unsigned b) { return a ^ b; }
__attribute__((noreturn)) void _start(void) {
    unsigned result = combine(60, 22);
    __asm__ volatile("" : : "a" (result));
    __asm__ volatile("hlt");
    __builtin_unreachable();
}

/* Original freestanding test; HLT is an interpreter sentinel, not a host program. */
__attribute__((noinline)) unsigned combine(unsigned value) {
    return ((value | 2u) ^ 16u) & 63u;
}
__attribute__((noreturn)) void _start(void) {
    unsigned result=combine(56u);
    __asm__ volatile("" : : "a" (result));
    __asm__ volatile("hlt");
    __builtin_unreachable();
}

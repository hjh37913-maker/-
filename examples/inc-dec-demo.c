/* Original freestanding example; synthetic HLT must not run on the host. */
__attribute__((noinline)) unsigned long adjust(unsigned long value) {
    __asm__ volatile("incq %0\n\tincq %0\n\tdecq %0" : "+r" (value) : : "cc");
    return value;
}
__attribute__((noreturn)) void _start(void) {
    unsigned long result=adjust(41);
    __asm__ volatile("" : : "a"(result));
    __asm__ volatile("hlt");
    __builtin_unreachable();
}

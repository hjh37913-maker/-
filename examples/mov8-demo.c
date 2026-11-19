/* Original freestanding example; HLT is a synthetic interpreter sentinel. */
static volatile unsigned char value;
__attribute__((noinline)) unsigned copy_byte(unsigned char x) {
    value = x;
    return value;
}
__attribute__((noreturn)) void _start(void) {
    unsigned result=copy_byte(42);
    __asm__ volatile("" : : "a" (result));
    __asm__ volatile("hlt");
    __builtin_unreachable();
}

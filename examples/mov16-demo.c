/* Original freestanding test: HLT is a synthetic interpreter sentinel. */
static volatile unsigned short value;
__attribute__((noinline)) unsigned set_word(unsigned short v) { value=v; return value; }
__attribute__((noreturn)) void _start(void) {
    unsigned result=set_word(42);
    __asm__ volatile("" : : "a" (result));
    __asm__ volatile("hlt");
    __builtin_unreachable();
}

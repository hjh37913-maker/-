/* Original freestanding example; HLT is a synthetic interpreter sentinel. */
__attribute__((noinline)) unsigned read_byte(const unsigned char *p) { return *p; }
static const unsigned char original[] = { 42 };
__attribute__((noreturn)) void _start(void) {
    unsigned result=read_byte(original);
    __asm__ volatile("" : : "a" (result));
    __asm__ volatile("hlt");
    __builtin_unreachable();
}

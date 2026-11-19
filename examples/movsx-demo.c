/* Original freestanding example: HLT is a synthetic interpreter sentinel. */
__attribute__((noinline)) long widen(const signed char *byte, const signed short *word, const signed int *dword) {
    return (long)*byte + (long)*word + (long)*dword;
}
static const signed char first=-10;
static const signed short second=-20;
static const signed int third=72;
__attribute__((noreturn)) void _start(void) {
    long result=widen(&first,&second,&third);
    __asm__ volatile("" : : "a"(result));
    __asm__ volatile("hlt");
    __builtin_unreachable();
}

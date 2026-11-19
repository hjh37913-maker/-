__attribute__((noinline)) unsigned long sum_to(unsigned long n) {
    unsigned long value=0;
    while(n) { value+=n; --n; }
    return value;
}
__attribute__((noreturn)) void _start(void) {
    volatile unsigned long result=sum_to(9);
    (void)result;
    __asm__ volatile("hlt");
    __builtin_unreachable();
}

__attribute__((noinline)) unsigned long add(unsigned long a, unsigned long b) { return a+b; }
__attribute__((noreturn)) void _start(void) { volatile unsigned long result = add(19,23); (void)result; __asm__ volatile("hlt"); __builtin_unreachable(); }

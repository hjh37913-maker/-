typedef unsigned (*fun)(unsigned);
__attribute__((noinline)) unsigned plus_two(unsigned x) { return x + 2; }
__attribute__((noreturn)) void _start(void) {
  fun volatile f = plus_two;
  unsigned value=f(40);
  __asm__ volatile("" : : "a" (value));
  __asm__ volatile("hlt"); __builtin_unreachable();
}

__attribute__((noinline)) unsigned long calculate(unsigned n) {
  unsigned x=0;
  while(n) { x+=n; n--; }
  return x;
}
__attribute__((noreturn)) void _start(void) {
  volatile unsigned long result=calculate(9);
  (void)result;
  __asm__ volatile("hlt");
  __builtin_unreachable();
}

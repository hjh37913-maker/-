__attribute__((noinline)) unsigned long sum(void) {
 volatile unsigned values[3];
 values[0]=10; values[1]=20; values[2]=12;
 return values[0]+values[1]+values[2];
}
__attribute__((noreturn)) void _start(void) {
 volatile unsigned long result=sum(); (void)result;
 __asm__ volatile("hlt"); __builtin_unreachable();
}

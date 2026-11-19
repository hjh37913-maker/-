/* Original freestanding program; its HLT is an interpreter stop sentinel. */
static volatile signed char input[] = {-10, 20, 32};
static volatile signed char output[3];
__attribute__((noinline)) long copy_and_sum(void) {
    long sum=0;
    for (unsigned i=0; i<3; ++i) {
        signed char value=input[i];
        output[i]=value;
        sum+=value;
    }
    return sum;
}
__attribute__((noreturn)) void _start(void) {
    long result=copy_and_sum();
    __asm__ volatile("" : : "a" (result));
    __asm__ volatile("hlt");
    __builtin_unreachable();
}

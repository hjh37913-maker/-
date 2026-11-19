/* Original freestanding Linux x86-64 FNV-1a test; no libc or protected data. */
typedef unsigned long u64;
typedef unsigned int u32;
static long call3(long number,long a,long b,long c) {
    long result;
    __asm__ volatile("syscall" : "=a"(result) : "a"(number),"D"(a),"S"(b),"d"(c) : "rcx","r11","memory");
    return result;
}
__attribute__((noreturn)) void _start(void) {
    unsigned char input[64]; char output[9];
    long n=call3(0,0,(long)input,sizeof(input));
    if(n<0) call3(60,1,0,0);
    u32 hash=2166136261u;
    for(long i=0;i<n;++i) hash=(hash^input[i])*16777619u;
    static const char digits[]="0123456789abcdef";
    for(int i=7;i>=0;--i) { output[i]=digits[hash&15]; hash>>=4; }
    output[8]='\n';
    call3(1,1,(long)output,9);
    call3(60,0,0,0);
    __builtin_unreachable();
}

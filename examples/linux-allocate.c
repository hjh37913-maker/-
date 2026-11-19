/* Original freestanding C test of anonymous guest pages and syscalls. */
static long sys6(long n,long a,long b,long c,long d,long e,long f) {
 register long r10 __asm__("r10")=d, r8 __asm__("r8")=e, r9 __asm__("r9")=f;
 long out;
 __asm__ volatile("syscall" : "=a"(out) : "a"(n),"D"(a),"S"(b),"d"(c),"r"(r10),"r"(r8),"r"(r9) : "rcx","r11","memory");
 return out;
}
__attribute__((noreturn,force_align_arg_pointer)) void _start(void) {
 unsigned char* page=(unsigned char*)sys6(9,0,4096,3,34,-1,0);
 if((long)page<0) sys6(60,1,0,0,0,0,0);
 long n=sys6(0,0,(long)page,64,0,0,0);
 if(n<0) sys6(60,2,0,0,0,0,0);
 if(sys6(10,(long)page,4096,1,0,0,0)) sys6(60,3,0,0,0,0,0);
 unsigned hash=2166136261u;for(long i=0;i<n;++i)hash=(hash^page[i])*16777619u;
 char out[9]; static const char digits[]="0123456789abcdef";
 for(int i=7;i>=0;--i){out[i]=digits[hash&15];hash>>=4;}out[8]='\n';
 sys6(1,1,(long)out,9,0,0,0);sys6(11,(long)page,4096,0,0,0,0);
 sys6(60,0,0,0,0,0,0);__builtin_unreachable();
}

/* Original static SSE2 test using unaligned vector loads and stores. */
#include <emmintrin.h>
static long call3(long number,long a,long b,long c) {
 long result; __asm__ volatile("syscall" : "=a"(result) : "a"(number),"D"(a),"S"(b),"d"(c) : "rcx","r11","memory"); return result;
}
__attribute__((noreturn,force_align_arg_pointer)) void _start(void) {
 unsigned char input[32], output[16]; long n=call3(0,0,(long)input,32);
 if(n!=32) call3(60,1,0,0);
 __m128i a=_mm_loadu_si128((const __m128i*)input), b=_mm_loadu_si128((const __m128i*)(input+16));
 __m128i result=_mm_xor_si128(_mm_add_epi32(a,b),b);
 _mm_storeu_si128((__m128i*)output,result);
 call3(1,1,(long)output,16); call3(60,0,0,0); __builtin_unreachable();
}

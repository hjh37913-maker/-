/* Original freestanding C guest file fixture; no libc calls or protected data. */
#include <sys/stat.h>
static long sys4(long n,long a,long b,long c,long d) {
 register long r10 __asm__("r10")=d; long result;
 __asm__ volatile("syscall" : "=a"(result) : "a"(n),"D"(a),"S"(b),"d"(c),"r"(r10) : "rcx","r11","memory");
 return result;
}
int guest_main(long argc,char** argv) {
 if(argc!=2)return 1;
 const long fd=sys4(257,-100,(long)argv[1],0,0); if(fd<0)return 2;
 struct stat metadata; if(sys4(5,fd,(long)&metadata,0,0))return 3;
 if(!S_ISREG(metadata.st_mode) || metadata.st_size<0 || metadata.st_size>64)return 4;
 char data[64]; long n=sys4(0,fd,(long)data,metadata.st_size,0); if(n!=metadata.st_size)return 5;
 if(sys4(1,1,(long)data,n,0)!=n)return 6;
 if(sys4(3,fd,0,0,0))return 7;
 return 0;
}

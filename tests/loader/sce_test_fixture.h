#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
inline void put(std::vector<std::uint8_t>&b,std::size_t p,std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b[p+i]=v>>(8*i);}
inline std::vector<std::uint8_t> fixture(){std::vector<std::uint8_t>b(0xc00);b[0]=0x7f;b[1]='E';b[2]='L';b[3]='F';b[4]=2;b[5]=1;b[6]=1;b[7]=9;
 put(b,16,0xfe10,2);put(b,18,62,2);put(b,20,1,4);put(b,24,0x1000,8);put(b,32,64,8);put(b,52,64,2);put(b,54,56,2);put(b,56,4,2);
 auto ph=[&](int i,std::uint32_t t,std::uint32_t flags,std::uint64_t off,std::uint64_t addr,std::uint64_t len){auto p=64+i*56;put(b,p,t,4);put(b,p+4,flags,4);put(b,p+8,off,8);put(b,p+16,addr,8);put(b,p+32,len,8);put(b,p+40,len,8);put(b,p+48,1,8);};
 ph(0,1,5,0x400,0x1000,0x40);ph(1,1,6,0x500,0x2000,0x40);ph(2,2,4,0x600,0,192);ph(3,0x61000000,4,0x800,0,0x400);
 const std::array<std::uint8_t,18> code{0x48,0x8b,0x05,0xf9,0x0f,0,0,0xff,0xd0,0x89,0xc7,0xb8,0x3c,0,0,0,0x0f,0x05};for(std::size_t i=0;i<code.size();++i)b[0x400+i]=code[i];
 const std::array<std::uint8_t,6> stub{0xb8,42,0,0,0,0xc3};for(std::size_t i=0;i<stub.size();++i)b[0x420+i]=stub[i];
 const std::array<std::pair<std::uint64_t,std::uint64_t>,12> tags{{{0x61000035,0},{0x61000037,32},{0x61000039,0x100},{0x6100003f,48},{0x6100003b,24},{0x6100002f,0x200},{0x61000031,48},{0x61000033,24},{0x61000029,0x230},{0x6100002d,24},{0x6100002b,7},{0,0}}};
 for(std::size_t i=0;i<tags.size();++i){put(b,0x600+16*i,tags[i].first,8);put(b,0x608+16*i,tags[i].second,8);}
 const std::string name="synthetic#A#B";for(std::size_t i=0;i<name.size();++i)b[0x801+i]=name[i];put(b,0x918,1,4);b[0x91c]=0x12; // undefined global func
 put(b,0xa00,0x2000,8);put(b,0xa08,(1ULL<<32)|6,8);
 put(b,0xa18,0x2008,8);put(b,0xa20,8,8);put(b,0xa28,0x1020,8);
 put(b,0xa30,0x2010,8);put(b,0xa38,(1ULL<<32)|7,8);return b;
}

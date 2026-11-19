#include "memory/memory.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace nightfall;
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
int main() {
 constexpr std::size_t capacity=32768; std::uint64_t rng=0x123fedab0765abc3ULL;
 for(unsigned round=0;round<100;++round) {
  Memory flat(capacity,false), sparse(capacity,SparsePages{8});
  if(!flat.map(0,capacity,Permission::read|Permission::write) || !sparse.map(0,capacity,Permission::read|Permission::write)) return 1;
  for(unsigned step=0;step<2000;++step) {
   const auto op=next(rng)%6; auto address=next(rng)%(capacity+128), len=next(rng)%129; if(step%10==0) address=(next(rng)%8)*4096-std::min<std::uint64_t>(next(rng)%64,32);
   const auto perm=static_cast<Permission>(next(rng)%8); bool a=false,b=false;
   if(op==0) { a=flat.map(address,len,perm); b=sparse.map(address,len,perm); }
   else if(op==1) { a=flat.unmap(address,len); b=sparse.unmap(address,len); }
   else if(op==2) { a=flat.protect(address,len,perm); b=sparse.protect(address,len,perm); }
   else {
    std::vector<std::uint8_t> lhs(len),rhs(len); for(std::size_t j=0;j<len;++j) lhs[j]=rhs[j]=std::uint8_t(next(rng));
    if(op==3) { a=flat.write(address,lhs); b=sparse.write(address,rhs); }
    else if(op==4) { a=flat.read(address,lhs); b=sparse.read(address,rhs); }
    else { a=flat.fetch(address,lhs); b=sparse.fetch(address,rhs); }
    if(lhs!=rhs) { std::cerr<<"data mismatch round="<<round<<" step="<<step<<'\n'; return 1; }
   }
   if(a!=b) { std::cerr<<"status mismatch round="<<round<<" step="<<step<<" op="<<op<<" address="<<address<<" len="<<len<<'\n'; return 1; }
   if(step%100==0) { auto x=flat.stats(),y=sparse.stats(); if(x.capacity!=y.capacity || x.mapped!=y.mapped || x.readable!=y.readable || x.writable!=y.writable || x.executable!=y.executable) { std::cerr<<"stats mismatch\n"; return 1; } }
  }
 }
 std::cout<<"200000 randomized flat/sparse memory operation comparisons passed\n";
}

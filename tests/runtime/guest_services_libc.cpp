#include "runtime/guest_services.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
using namespace nightfall;using namespace nightfall::runtime;
int main(){std::uint64_t seed=0x985ab224416edd63ULL;auto rnd=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};GuestServices services;
 const std::array kinds{ServiceKind::copy,ServiceKind::move,ServiceKind::fill,ServiceKind::compare,ServiceKind::string_length};
 for(unsigned i=0;i<kinds.size();++i)if(!services.add("op"+std::to_string(i),1500+i,kinds[i]))return 1;
 unsigned comparisons=0;
 for(unsigned kind=0;kind<5;++kind)for(unsigned case_id=0;case_id<8192;++case_id){
  std::array<std::uint8_t,512> reference{},actual{};for(auto& value:reference)value=static_cast<std::uint8_t>(rnd());auto original=reference;
  std::size_t n=rnd()%129,a=rnd()%(513-n),b=rnd()%(513-n);if(kind==0){a=rnd()%128;b=256+rnd()%128;}
  int cmp=0;std::size_t length=0;
  if(kind==0)std::memcpy(reference.data()+a,reference.data()+b,n);
  else if(kind==1)std::memmove(reference.data()+a,reference.data()+b,n);
  else if(kind==2)std::memset(reference.data()+a,static_cast<int>(b),n);
  else if(kind==3)cmp=std::memcmp(reference.data()+a,reference.data()+b,n);
  else{a=rnd()%384;const auto wanted=rnd()%128;for(std::size_t j=0;j<wanted;++j)original[a+j]=static_cast<std::uint8_t>(1+rnd()%255);original[a+wanted]=0;reference=original;length=std::strlen(reinterpret_cast<const char*>(reference.data()+a));}
  Machine machine(2048);if(!machine.memory.write(0,original))return 1;std::array<std::uint8_t,8> ret{0x20};if(!machine.memory.write(1024,ret))return 1;
  machine.cpu.state.rip=1500+kind;*machine.cpu.state.reg(4)=1024;*machine.cpu.state.reg(7)=a;*machine.cpu.state.reg(6)=kind==2?b:b;*machine.cpu.state.reg(2)=n;
  if(services.dispatch(machine)!=ServiceStatus::completed||machine.cpu.state.rip!=32||*machine.cpu.state.reg(4)!=1032||!machine.memory.read(0,actual)||actual!=reference){std::cerr<<"libc memory/result mismatch kind="<<kind<<" case="<<case_id<<'\n';return 1;}
  const auto value=machine.cpu.state.rax;
  if((kind<3&&value!=a)||(kind==3&&((cmp==0&&value!=0)||(cmp<0&&value!=UINT64_MAX)||(cmp>0&&value!=1)))||(kind==4&&value!=length)){std::cerr<<"libc return mismatch kind="<<kind<<" case="<<case_id<<'\n';return 1;}
  ++comparisons;
 }
 std::cout<<comparisons<<" guest service cases matched host C library semantics; guest addressing not native\n";
}

#include "runtime/guest_services.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;using namespace nightfall::runtime;
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<": " #x "\n";return 1;}}while(0)
void put64(std::array<std::uint8_t,8>&a,std::uint64_t v){for(unsigned i=0;i<8;++i)a[i]=v>>(i*8);}
int main(){Machine m(1024);GuestServices table;CHECK(table.add("copy",900,ServiceKind::copy)&&table.add("move",901,ServiceKind::move)&&table.add("fill",902,ServiceKind::fill)&&table.add("compare",903,ServiceKind::compare)&&table.add("length",904,ServiceKind::string_length));
 CHECK(!table.add("copy",905,ServiceKind::copy)&&!table.add("new",904,ServiceKind::copy));auto bindings=table.bindings();CHECK(bindings.size()==5&&bindings[1].name=="move"&&bindings[1].guest_address==901);
 const std::array<std::uint8_t,8> input{'a','b','c','d','e','f',0,0};std::array<std::uint8_t,8> ret{};put64(ret,77);CHECK(m.memory.write(100,input)&&m.memory.write(800,ret));
 auto setup=[&](std::uint64_t service,std::uint64_t a,std::uint64_t b,std::uint64_t c){m.cpu.state.rip=service;*m.cpu.state.reg(4)=800;*m.cpu.state.reg(7)=a;*m.cpu.state.reg(6)=b;*m.cpu.state.reg(2)=c;m.cpu.state.rax=0x1234;};
 setup(900,200,100,6);CHECK(table.dispatch(m)==ServiceStatus::completed&&m.cpu.state.rax==200&&m.cpu.state.rip==77&&*m.cpu.state.reg(4)==808);std::array<std::uint8_t,8> out{};CHECK(m.memory.read(200,out)&&out==input);
 setup(901,102,100,4);CHECK(table.dispatch(m)==ServiceStatus::completed);CHECK(m.memory.read(100,out)&&(out==std::array<std::uint8_t,8>{'a','b','a','b','c','d',0,0}));
 setup(900,101,100,5);CHECK(table.dispatch(m)==ServiceStatus::unsupported&&m.cpu.state.rip==900&&m.cpu.state.rax==0x1234);
 setup(902,200,0x1234,3);CHECK(table.dispatch(m)==ServiceStatus::completed&&m.cpu.state.rax==200);CHECK(m.memory.read(200,out)&&out[0]==0x34&&out[2]==0x34&&out[3]=='d');
 setup(903,100,100,6);CHECK(table.dispatch(m)==ServiceStatus::completed&&m.cpu.state.rax==0);setup(903,100,200,1);CHECK(table.dispatch(m)==ServiceStatus::completed&&m.cpu.state.rax==1);
 setup(904,100,0,0);CHECK(table.dispatch(m)==ServiceStatus::completed&&m.cpu.state.rax==6);
 setup(900,UINT64_MAX,UINT64_MAX,0);CHECK(table.dispatch(m)==ServiceStatus::completed&&m.cpu.state.rax==UINT64_MAX);
 setup(900,200,100,65537);CHECK(table.dispatch(m)==ServiceStatus::resource_limit&&m.cpu.state.rip==900&&m.cpu.state.rax==0x1234);
 setup(901,1023,100,4);CHECK(table.dispatch(m)==ServiceStatus::memory_fault&&m.cpu.state.rip==901&&m.cpu.state.rax==0x1234);
 setup(901,200,100,4);*m.cpu.state.reg(4)=UINT64_MAX;CHECK(table.dispatch(m)==ServiceStatus::memory_fault&&m.cpu.state.rip==901);
 // A failed source, destination, or return-slot access must not modify registers or memory.
 std::array<std::uint8_t,1024> before{},after{};CHECK(m.memory.read(0,before));
 setup(901,200,1022,4);CHECK(table.dispatch(m)==ServiceStatus::memory_fault&&m.cpu.state.rip==901&&m.cpu.state.rax==0x1234&&*m.cpu.state.reg(4)==800);CHECK(m.memory.read(0,after)&&after==before);
 setup(902,1022,0xaa,4);CHECK(table.dispatch(m)==ServiceStatus::memory_fault&&m.cpu.state.rip==902&&m.cpu.state.rax==0x1234);CHECK(m.memory.read(0,after)&&after==before);
 setup(904,1023,0,0);std::array<std::uint8_t,1> nonzero{1};CHECK(m.memory.write(1023,nonzero));std::array<std::uint8_t,1024> before_length{};CHECK(m.memory.read(0,before_length));CHECK(table.dispatch(m)==ServiceStatus::memory_fault&&m.cpu.state.rip==904&&m.cpu.state.rax==0x1234);CHECK(m.memory.read(0,after)&&after==before_length);
 setup(999,0,0,0);CHECK(table.dispatch(m)==ServiceStatus::not_service);
 // Destination overlaps the return slot; the return uses the updated bytes.
 std::array<std::uint8_t,8> newret{};put64(newret,456);CHECK(m.memory.write(300,newret));setup(901,800,300,8);CHECK(table.dispatch(m)==ServiceStatus::completed&&m.cpu.state.rip==456);
 // Bytecode: call service via RAX; after its guest RET, HLT.
 const std::array<std::uint8_t,13> code{0x48,0xb8,0x88,0x03,0,0,0,0,0,0,0xff,0xd0,0xf4};CHECK(m.memory.write(0,code));m.cpu.state.rip=0;*m.cpu.state.reg(4)=800;*m.cpu.state.reg(7)=100;
 auto report=run_with_services(m,table,10);CHECK(report.result==GuestRunResult::halted&&report.attempted==4&&m.cpu.state.rax==6&&*m.cpu.state.reg(4)==800);
 m.cpu.state.rip=0;*m.cpu.state.reg(4)=800;CHECK(run_with_services(m,table,2).result==GuestRunResult::budget_exhausted&&m.cpu.state.rip==904);
 std::cout<<"guest services ABI, faults, overlap, bounded execution and return behavior passed\n";
}

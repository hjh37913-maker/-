#include "core/machine.h"
#include "loader/elf64.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>
#include <utility>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
void put(std::vector<std::uint8_t>& b,std::size_t p,std::uint64_t v,unsigned n) { for(unsigned i=0;i<n;++i) b[p+i]=v>>(8*i); }
int main() {
 // The high-address tests require a 64-bit host; cap setup is not a platform claim.
 if(sizeof(std::size_t)<8) { std::cout<<"Sparse high addresses require 64-bit size_t\n"; return 0; }
 const std::uint64_t high=0x10000000000ULL; const auto capacity=static_cast<std::size_t>(high+0x40000);
 Memory m(capacity,SparsePages{3}); CHECK(m.is_sparse() && m.size()==capacity && !m.allocated_pages());
 CHECK(m.map_detailed(high+4094,6,Permission::read|Permission::write)==MapStatus::ok && m.allocated_pages()==2);
 const std::array<std::uint8_t,6> data{1,2,3,4,5,6}; CHECK(m.write(high+4094,data)); std::array<std::uint8_t,6> out{};
 CHECK(m.read(high+4094,out) && out==data); CHECK(m.stats().mapped==6 && m.stats().readable==6 && m.stats().writable==6);
 CHECK(m.map_detailed(high+4093,2,Permission::read)==MapStatus::overlap && m.stats().mapped==6);
 CHECK(m.map_detailed(high+0x3000,0x2000,Permission::read)==MapStatus::resource_limit && m.allocated_pages()==2 && m.stats().mapped==6);
 CHECK(m.protect(high+4096,1,Permission::read)); const auto before=out; CHECK(!m.write(high+4094,data)); CHECK(m.read(high+4094,out) && out==before);
 CHECK(!m.unmap(high+4093,2) && m.stats().mapped==6); CHECK(m.unmap(high+4094,6) && m.allocated_pages()==0);
 CHECK(m.map(high+4094,6,Permission::read|Permission::write) && m.read(high+4094,out) && (out==std::array<std::uint8_t,6>{}));
 CHECK(m.unmap(high+4094,6)); CHECK(m.map_detailed(capacity,1,Permission::read)==MapStatus::invalid);
 std::span<std::uint8_t> empty; CHECK(m.read(capacity,empty) && !m.read(capacity+1,empty));
 // Partially used pages are released only when their last mapping disappears.
 CHECK(m.map(high,1,Permission::read) && m.map(high+100,1,Permission::read)); CHECK(m.unmap(high,1) && m.allocated_pages()==1);
 CHECK(m.unmap(high+100,1) && m.allocated_pages()==0);
 // Read/fetch faults across pages are all-or-nothing for output buffers.
 { Memory access(capacity,SparsePages{2}); CHECK(access.map(high+4094,4,Permission::read|Permission::write));
   const std::array<std::uint8_t,4> source{1,2,3,4}; CHECK(access.write(high+4094,source)); CHECK(access.protect(high+4096,2,Permission::execute));
   std::array<std::uint8_t,4> dest{9,9,9,9}; const auto sentinel=dest;
   CHECK(!access.read(high+4094,dest) && dest==sentinel); CHECK(!access.fetch(high+4094,dest) && dest==sentinel);
   CHECK(access.fetch(high+4096,std::span(dest).first(2)) && dest[0]==3 && dest[1]==4);
   CHECK(access.map_detailed(high+0x3000,1,Permission::read)==MapStatus::resource_limit && access.allocated_pages()==2);
 }
 // Large ranges fail by page budget without attempting proportional allocations.
 { Memory capped(capacity,SparsePages{0}); CHECK(capped.map_detailed(0,capacity,Permission::read)==MapStatus::resource_limit && capped.allocated_pages()==0);
   CHECK(capped.map_detailed(0,1,static_cast<Permission>(0x80))==MapStatus::invalid); }
 // Moved-from flat and sparse memory objects remain safely empty and reusable.
 for(bool sparse:{false,true}) {
   Memory original=sparse?Memory(8192,SparsePages{2}):Memory(8192,false);
   CHECK(original.map(0,1,Permission::read|Permission::write)); const std::array<std::uint8_t,1> value{42}; CHECK(original.write(0,value));
   Memory moved(std::move(original)); CHECK(original.size()==0 && !original.is_sparse() && original.stats().mapped==0);
   std::array<std::uint8_t,1> byte{}; CHECK(!original.read(0,byte) && moved.read(0,byte) && byte==value);
   original=std::move(moved); CHECK(moved.size()==0 && !moved.read(0,byte) && original.read(0,byte) && byte==value);
 }
 // Real CPU execution from a high address on the sparse backend.
 { Machine machine(capacity,SparsePages{2}); const std::array<std::uint8_t,17> code{0x48,0xb8,40,0,0,0,0,0,0,0,0x48,0x05,2,0,0,0,0xf4};
   CHECK(machine.memory.map(high,code.size(),Permission::read|Permission::write)); CHECK(machine.memory.write(high,code));
   CHECK(machine.memory.protect(high,code.size(),Permission::read|Permission::execute)); machine.cpu.state.rip=high;
   CHECK(machine.run(3)==RunResult::halted && machine.cpu.state.rax==42 && machine.cpu.state.rip==high+code.size()); }
 // Original static ELF at a high virtual address; loader preserves sparse policy.
 { std::vector<std::uint8_t> f(0x111); f[0]=0x7f; f[1]='E'; f[2]='L'; f[3]='F'; f[4]=2; f[5]=1; f[6]=1;
   put(f,16,2,2); put(f,18,62,2); put(f,20,1,4); put(f,24,high,8); put(f,32,64,8); put(f,52,64,2); put(f,54,56,2); put(f,56,1,2);
   put(f,64,1,4); put(f,68,5,4); put(f,72,0x100,8); put(f,80,high,8); put(f,96,17,8); put(f,104,17,8); put(f,112,1,8);
   const std::array<std::uint8_t,17> code{0x48,0xb8,40,0,0,0,0,0,0,0,0x48,0x05,2,0,0,0,0xf4}; std::copy(code.begin(),code.end(),f.begin()+0x100);
   Machine machine(capacity,SparsePages{2}); CHECK(loader::load_elf64(f,machine,high+0x10000,4096)==loader::ElfStatus::ok);
   CHECK(machine.memory.is_sparse() && machine.memory.allocated_pages()==2 && machine.run(3)==RunResult::halted && machine.cpu.state.rax==42);
   Machine limited(capacity,SparsePages{1}); CHECK(limited.memory.map(0,1,Permission::read|Permission::write)); limited.cpu.state.rax=123;
   CHECK(loader::load_elf64(f,limited,high+0x10000,4096)==loader::ElfStatus::resource_limit && limited.cpu.state.rax==123 && limited.memory.stats().mapped==1);
 }
}

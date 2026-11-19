#include "memory/memory.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace nightfall;
namespace {
constexpr std::size_t capacity=64;
// Independent, deliberately simple reference: -1 is unmapped, 0..7 are permissions.
struct Cell { int permission=-1; std::uint8_t value=0; };
std::uint64_t next(std::uint64_t& x) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; return x; }
bool range_ok(std::uint64_t addr, std::size_t n) { return addr<=capacity && n<=capacity-static_cast<std::size_t>(addr); }
bool matches(const std::array<Cell,capacity>& model, std::uint64_t addr, std::size_t n, int required) {
    if (!range_ok(addr,n)) return false;
    for (std::size_t i=0; i<n; ++i) {
        const int p=model[static_cast<std::size_t>(addr)+i].permission;
        if (p<0 || (p & required)!=required) return false;
    }
    return true;
}
bool full_state(const Memory& memory, const std::array<Cell,capacity>& cells) {
    std::size_t mapped=0, read=0, write=0, execute=0;
    for (std::size_t i=0; i<capacity; ++i) {
        const int p=cells[i].permission;
        if (p<0) continue;
        ++mapped; read+=(p&1)!=0; write+=(p&2)!=0; execute+=(p&4)!=0;
        if (p&1) { std::array<std::uint8_t,1> b{}; if (!memory.read(i,b) || b[0]!=cells[i].value) return false; }
        if (p&4) { std::array<std::uint8_t,1> b{}; if (!memory.fetch(i,b) || b[0]!=cells[i].value) return false; }
    }
    auto s=memory.stats();
    return s.capacity==capacity && s.mapped==mapped && s.readable==read && s.writable==write && s.executable==execute;
}
}
int main() {
    std::array<std::array<std::size_t,2>,6> coverage{};
    for (std::uint64_t seed=1; seed<=100; ++seed) {
        std::uint64_t random=0x4e4947485446414cULL ^ (seed * 0x9e3779b97f4a7c15ULL);
        Memory memory(capacity,false); std::array<Cell,capacity> cells{};
        for (std::size_t step=0; step<500; ++step) {
            const int op=next(random)%6;
            const std::uint64_t addr=(next(random)%20==0) ? UINT64_MAX : next(random)%72;
            const std::size_t count=next(random)%9;
            const int permission=next(random)%10; // includes invalid flags 8 and 9
            const auto perm=static_cast<Permission>(permission);
            bool expected=false, actual=false;
            std::vector<std::uint8_t> bytes(count,0xcc), original=bytes;
            if (op==0) {
                expected=count && range_ok(addr,count) && permission<8;
                if (expected) for (std::size_t i=0;i<count;++i) if (cells[addr+i].permission>=0) { expected=false; break; }
                actual=memory.map(addr,count,perm);
                if (expected) for (std::size_t i=0;i<count;++i) cells[addr+i]={permission,0};
            } else if (op==1) {
                expected=count && matches(cells,addr,count,0);
                actual=memory.unmap(addr,count);
                if (expected) for (std::size_t i=0;i<count;++i) cells[addr+i]={-1,0};
            } else if (op==2) {
                expected=count && permission<8 && matches(cells,addr,count,0);
                actual=memory.protect(addr,count,perm);
                if (expected) for (std::size_t i=0;i<count;++i) cells[addr+i].permission=permission;
            } else if (op==3) {
                expected=matches(cells,addr,count,2);
                for (auto& b:bytes) b=static_cast<std::uint8_t>(next(random));
                actual=memory.write(addr,bytes);
                if (expected) for (std::size_t i=0;i<count;++i) cells[addr+i].value=bytes[i];
            } else {
                expected=matches(cells,addr,count,op==4 ? 1 : 4);
                actual=op==4 ? memory.read(addr,bytes) : memory.fetch(addr,bytes);
                if (expected) for (std::size_t i=0;i<count;++i) original[i]=cells[addr+i].value;
                if (bytes!=original) { std::cerr << "Output mismatch seed=" << seed << " step=" << step << '\n'; return 1; }
            }
            ++coverage[op][actual ? 1 : 0];
            if (actual!=expected || !full_state(memory,cells)) {
                std::cerr << "Model mismatch seed=" << seed << " step=" << step << " op=" << op
                          << " addr=" << addr << " count=" << count << " permissions=" << permission << '\n'; return 1;
            }
        }
    }
    for (std::size_t op=0;op<coverage.size();++op) {
        if (!coverage[op][0] || !coverage[op][1]) { std::cerr << "Missing success/failure coverage for op " << op << '\n'; return 1; }
        std::cout << "operation " << op << ": " << coverage[op][1] << " success, " << coverage[op][0] << " failure\n";
    }
    std::cout << "100 reproducible memory-model scenarios, 500 operations each passed\n";
}

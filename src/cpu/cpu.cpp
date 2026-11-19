#include "cpu/cpu.h"
#include <array>
#include <bit>
namespace nightfall {
namespace {
std::uint64_t little(const std::uint8_t* p, unsigned n) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < n; ++i) value |= std::uint64_t(p[i]) << (8 * i);
    return value;
}
std::uint64_t sign_extend(std::uint64_t value, unsigned bits) {
    const std::uint64_t sign=std::uint64_t{1} << (bits-1);
    return (value & sign) ? value | (~std::uint64_t{0} << bits) : value;
}
std::array<std::uint8_t,8> bytes64(std::uint64_t value) {
    std::array<std::uint8_t,8> bytes{};
    for (unsigned i=0;i<8;++i) bytes[i]=static_cast<std::uint8_t>(value>>(8*i));
    return bytes;
}
// Decode 64-bit effective-address components for a ModRM memory operand.
// trailing is the number of bytes after displacement (e.g. MOV imm32), which
// are part of the instruction length used by RIP-relative addressing.
bool effective_address(const Memory& memory, const CpuState& state, std::array<std::uint8_t,15>& bytes,
                       unsigned modrm_pos, std::uint8_t rex, unsigned trailing,
                       std::uint64_t& address, unsigned& length) {
    const auto mod=bytes[modrm_pos]>>6, low_rm=bytes[modrm_pos]&7;
    length=modrm_pos+1;
    std::uint64_t base=0,index=0;
    bool rip_relative=false,no_base=false;
    if (low_rm==4) {
        if (!memory.fetch(state.rip,std::span(bytes).first(length+1))) return false;
        const auto sib=bytes[length++];
        const unsigned low_index=(sib>>3)&7;
        if (low_index!=4 || (rex&2)) index=*state.reg(low_index+((rex&2)?8:0)) << (sib>>6);
        const unsigned low_base=sib&7;
        no_base=mod==0 && low_base==5;
        if (!no_base) base=*state.reg(low_base+((rex&1)?8:0));
    } else if (mod==0 && low_rm==5) rip_relative=true;
    else base=*state.reg(low_rm+((rex&1)?8:0));
    const unsigned disp_size=mod==1 ? 1 : ((mod==2 || rip_relative || no_base) ? 4 : 0);
    const unsigned disp_offset=length;
    length+=disp_size+trailing;
    if (!memory.fetch(state.rip,std::span(bytes).first(length))) return false;
    const auto displacement=disp_size ? sign_extend(little(bytes.data()+disp_offset,disp_size),disp_size*8) : 0;
    if (rip_relative) base=state.rip+length;
    address=base+index+displacement;
    return true;
}
bool condition(unsigned code, std::uint64_t flags) {
    const bool cf=flags & (1ULL<<0), pf=flags & (1ULL<<2), zf=flags & (1ULL<<6),
               sf=flags & (1ULL<<7), of=flags & (1ULL<<11);
    bool positive=false;
    switch (code>>1) {
    case 0: positive=of; break;
    case 1: positive=cf; break;
    case 2: positive=zf; break;
    case 3: positive=cf||zf; break;
    case 4: positive=sf; break;
    case 5: positive=pf; break;
    case 6: positive=sf!=of; break;
    case 7: positive=zf||(sf!=of); break;
    }
    return (code&1) ? !positive : positive;
}
void logical_test(CpuState& state,std::uint64_t value) {
    // AF is architecturally undefined for TEST; this model preserves it.
    constexpr std::uint64_t mask=(1ULL<<0)|(1ULL<<2)|(1ULL<<6)|(1ULL<<7)|(1ULL<<11);
    std::uint64_t flags=0;
    if (std::popcount(static_cast<unsigned>(value&0xff))%2==0) flags|=1ULL<<2;
    if (!value) flags|=1ULL<<6;
    if (value>>63) flags|=1ULL<<7;
    state.rflags=(state.rflags&~mask)|flags;
}
void arithmetic32(CpuState& state, std::uint64_t& destination, std::uint32_t rhs, bool subtract, bool compare) {
    const auto lhs=static_cast<std::uint32_t>(destination);
    const std::uint32_t result=subtract ? lhs-rhs : lhs+rhs;
    constexpr std::uint64_t mask=(1ULL<<0)|(1ULL<<2)|(1ULL<<4)|(1ULL<<6)|(1ULL<<7)|(1ULL<<11);
    std::uint64_t flags=0;
    if (subtract ? lhs<rhs : result<lhs) flags|=1ULL<<0;
    if (std::popcount(static_cast<unsigned>(result&0xff))%2==0) flags|=1ULL<<2;
    if ((lhs^rhs^result)&0x10) flags|=1ULL<<4;
    if (!result) flags|=1ULL<<6;
    if (result>>31) flags|=1ULL<<7;
    if (((subtract ? (lhs^rhs) : ~(lhs^rhs)) & (lhs^result))>>31) flags|=1ULL<<11;
    if (!compare) destination=result; // 32-bit writes zero-extend.
    state.rflags=(state.rflags&~mask)|flags;
}
void logical_test32(CpuState& state, std::uint32_t value) {
    constexpr std::uint64_t mask=(1ULL<<0)|(1ULL<<2)|(1ULL<<6)|(1ULL<<7)|(1ULL<<11);
    std::uint64_t flags=0;
    if (std::popcount(static_cast<unsigned>(value&0xff))%2==0) flags|=1ULL<<2;
    if (!value) flags|=1ULL<<6;
    if (value>>31) flags|=1ULL<<7;
    state.rflags=(state.rflags&~mask)|flags; // AF undefined: preserve it.
}
void arithmetic(CpuState& state, std::uint64_t& destination, std::uint64_t rhs, bool subtract, bool compare) {
    const auto lhs = destination;
    const auto result = subtract ? lhs-rhs : lhs+rhs;
    constexpr std::uint64_t mask = (1ULL<<0)|(1ULL<<2)|(1ULL<<4)|(1ULL<<6)|(1ULL<<7)|(1ULL<<11);
    std::uint64_t flags = 0;
    if (subtract ? lhs < rhs : result < lhs) flags |= 1ULL<<0;
    if (std::popcount(static_cast<unsigned>(result & 0xff)) % 2 == 0) flags |= 1ULL<<2;
    if ((lhs ^ rhs ^ result) & 0x10) flags |= 1ULL<<4;
    if (result == 0) flags |= 1ULL<<6;
    if (result >> 63) flags |= 1ULL<<7;
    if (((subtract ? (lhs ^ rhs) : ~(lhs ^ rhs)) & (lhs ^ result)) >> 63) flags |= 1ULL<<11;
    if (!compare) destination = result;
    state.rflags = (state.rflags & ~mask) | flags;
}
// Group-1 logical extensions: OR /1, AND /4, XOR /6.
bool group_logical(unsigned extension) { return extension==1 || extension==4 || extension==6; }
void group_logical32(CpuState& state, std::uint64_t& dst, std::uint32_t rhs, unsigned extension) {
    const auto lhs=static_cast<std::uint32_t>(dst);
    dst=extension==1 ? lhs|rhs : extension==4 ? lhs&rhs : lhs^rhs;
    logical_test32(state,static_cast<std::uint32_t>(dst));
}
void group_logical64(CpuState& state, std::uint64_t& dst, std::uint64_t rhs, unsigned extension) {
    if (extension==1) dst|=rhs; else if (extension==4) dst&=rhs; else dst^=rhs;
    logical_test(state,dst);
}
}
StepResult Cpu::step(Memory& memory) {
    std::array<std::uint8_t, 15> b{};
    if (!memory.fetch(state.rip, std::span(b).first(1))) return StepResult::memory_fault;
    if (b[0] == 0x90 || b[0] == 0xf4) {
        ++state.rip;
        return b[0] == 0xf4 ? StepResult::halted : StepResult::running;
    }
    // Near indirect CALL/JMP and PUSH r/m64 (FF /2, /4, /6).
    // Evaluate memory/register operands before changing RSP, including [rsp].
    const bool rex_indirect=b[0]>=0x40 && b[0]<=0x4f;
    if (b[0]==0xff || rex_indirect) {
        const unsigned op_pos=rex_indirect?1:0;
        if (!memory.fetch(state.rip,std::span(b).first(op_pos+1))) return StepResult::memory_fault;
        if (b[op_pos]==0xff) {
            const unsigned modrm_pos=op_pos+1;
            if (!memory.fetch(state.rip,std::span(b).first(modrm_pos+1))) return StepResult::memory_fault;
            const unsigned mod=b[modrm_pos]>>6, extension=(b[modrm_pos]>>3)&7;
            if (extension!=2 && extension!=4 && extension!=6) return StepResult::invalid_instruction;
            const std::uint8_t rex=rex_indirect?b[0]:0;
            unsigned length=modrm_pos+1; std::uint64_t operand=0;
            if (mod==3) operand=*state.reg((b[modrm_pos]&7)+((rex&1)?8:0));
            else {
                std::uint64_t address=0;
                if (!effective_address(memory,state,b,modrm_pos,rex,0,address,length)) return StepResult::memory_fault;
                std::array<std::uint8_t,8> data{};
                if (!memory.read(address,data)) return StepResult::memory_fault;
                operand=little(data.data(),8);
            }
            if (extension==2 || extension==6) {
                auto& rsp=*state.reg(4); const auto next_rsp=rsp-8;
                const auto value=extension==2 ? state.rip+length : operand;
                if (!memory.write(next_rsp,bytes64(value))) return StepResult::memory_fault;
                rsp=next_rsp;
            }
            state.rip=extension==6 ? state.rip+length : operand;
            return StepResult::running;
        }
    }
    // MOV r32, imm32 zero-extends into the corresponding 64-bit register.
    if (b[0]>=0xb8 && b[0]<=0xbf) {
        if (!memory.fetch(state.rip,std::span(b).first(5))) return StepResult::memory_fault;
        *state.reg(b[0]-0xb8)=little(b.data()+1,4);
        state.rip+=5;
        return StepResult::running;
    }
    // Synthetic 64-bit stack instructions. RSP is register index 4.
    const bool extended_stack=b[0]==0x41;
    if (extended_stack && !memory.fetch(state.rip,std::span(b).first(2))) return StepResult::memory_fault;
    if (extended_stack && b[1]>=0xb8 && b[1]<=0xbf) {
        if (!memory.fetch(state.rip,std::span(b).first(6))) return StepResult::memory_fault;
        *state.reg(8+b[1]-0xb8)=little(b.data()+2,4);
        state.rip+=6;
        return StepResult::running;
    }
    const auto stack_opcode=extended_stack ? b[1] : b[0];
    if ((stack_opcode>=0x50 && stack_opcode<=0x5f) && (!extended_stack || b[0]==0x41)) {
        const unsigned index=(stack_opcode&7)+(extended_stack?8:0);
        auto& rsp=*state.reg(4);
        const unsigned length=extended_stack?2:1;
        if (stack_opcode<0x58) {
            const auto value=*state.reg(index); // PUSH RSP uses the old value.
            const auto new_rsp=rsp-8;
            if (!memory.write(new_rsp,bytes64(value))) return StepResult::memory_fault;
            rsp=new_rsp;
        } else {
            std::array<std::uint8_t,8> value{};
            if (!memory.read(rsp,value)) return StepResult::memory_fault;
            const auto new_rsp=rsp+8;
            const auto popped=little(value.data(),8);
            rsp=new_rsp;
            *state.reg(index)=popped; // POP RSP overrides the increment.
        }
        state.rip+=length;
        return StepResult::running;
    }
    // PUSH imm8/imm32 in long mode sign-extends to the 64-bit stack slot.
    if (!extended_stack && (stack_opcode==0x6a || stack_opcode==0x68)) {
        const unsigned width=stack_opcode==0x6a?1:4;
        if (!memory.fetch(state.rip,std::span(b).first(1+width))) return StepResult::memory_fault;
        auto& rsp=*state.reg(4); const auto next_rsp=rsp-8;
        const auto value=sign_extend(little(b.data()+1,width),width*8);
        if (!memory.write(next_rsp,bytes64(value))) return StepResult::memory_fault;
        rsp=next_rsp; state.rip+=1+width;
        return StepResult::running;
    }
    // LEAVE: RSP=RBP; POP RBP. Faults leave both registers unchanged.
    if (!extended_stack && stack_opcode==0xc9) {
        const auto frame=*state.reg(5); std::array<std::uint8_t,8> value{};
        if (!memory.read(frame,value)) return StepResult::memory_fault;
        *state.reg(4)=frame+8; *state.reg(5)=little(value.data(),8); ++state.rip;
        return StepResult::running;
    }
    // RET imm16 also discards an unsigned number of argument bytes.
    if (!extended_stack && stack_opcode==0xc2) {
        if (!memory.fetch(state.rip,std::span(b).first(3))) return StepResult::memory_fault;
        auto& rsp=*state.reg(4); std::array<std::uint8_t,8> address{};
        if (!memory.read(rsp,address)) return StepResult::memory_fault;
        state.rip=little(address.data(),8); rsp+=8+little(b.data()+1,2);
        return StepResult::running;
    }
    if (!extended_stack && stack_opcode==0xe8) { // CALL rel32
        if (!memory.fetch(state.rip,std::span(b).first(5))) return StepResult::memory_fault;
        const auto return_address=state.rip+5;
        const auto target=return_address+sign_extend(little(b.data()+1,4),32);
        auto& rsp=*state.reg(4);
        const auto new_rsp=rsp-8;
        if (!memory.write(new_rsp,bytes64(return_address))) return StepResult::memory_fault;
        rsp=new_rsp; state.rip=target;
        return StepResult::running;
    }
    if (!extended_stack && stack_opcode==0xc3) { // RET near
        auto& rsp=*state.reg(4); std::array<std::uint8_t,8> address{};
        if (!memory.read(rsp,address)) return StepResult::memory_fault;
        state.rip=little(address.data(),8); rsp+=8;
        return StepResult::running;
    }
    if (extended_stack) return StepResult::invalid_instruction;
    if (b[0]==0xe9) { // JMP rel32
        if (!memory.fetch(state.rip,std::span(b).first(5))) return StepResult::memory_fault;
        state.rip+=5+sign_extend(little(b.data()+1,4),32);
        return StepResult::running;
    }
    // Short JMP/Jcc and near Jcc; targets are checked on the next fetch.
    if (b[0]==0xeb || (b[0]>=0x70 && b[0]<=0x7f)) {
        if (!memory.fetch(state.rip,std::span(b).first(2))) return StepResult::memory_fault;
        const bool taken=b[0]==0xeb || condition(b[0]&15,state.rflags);
        state.rip+=2;
        if (taken) state.rip+=sign_extend(b[1],8);
        return StepResult::running;
    }
    if (b[0]==0x0f) {
        if (!memory.fetch(state.rip,std::span(b).first(2))) return StepResult::memory_fault;
        if (b[1]<0x80 || b[1]>0x8f) return StepResult::invalid_instruction;
        if (!memory.fetch(state.rip,std::span(b).first(6))) return StepResult::memory_fault;
        const bool taken=condition(b[1]&15,state.rflags);
        state.rip+=6;
        if (taken) state.rip+=sign_extend(little(b.data()+2,4),32);
        return StepResult::running;
    }
    // Selected unprefixed 32-bit register and memory forms.
    const auto op32=b[0];
    if (op32==0x81 || op32==0x83 || op32==0x85 || op32==0x89 || op32==0x8b || op32==0x8d || op32==0xc7 || op32==0x01 || op32==0x03 ||
        op32==0x29 || op32==0x2b || op32==0x31 || op32==0x33 || op32==0x09 || op32==0x0b || op32==0x21 || op32==0x23 || op32==0x39 || op32==0x3b) {
        if (!memory.fetch(state.rip,std::span(b).first(2))) return StepResult::memory_fault;
        const unsigned mod=b[1]>>6, reg=(b[1]>>3)&7, rm=b[1]&7;
        if (mod!=3) {
            if ((op32==0xc7 && reg!=0) ||
                ((op32==0x81 || op32==0x83) && reg!=0 && reg!=5 && reg!=7 && !group_logical(reg))) return StepResult::invalid_instruction;
            std::uint64_t address=0; unsigned length=0;
            const unsigned trailing=(op32==0xc7 || op32==0x81)?4:(op32==0x83?1:0);
            if (!effective_address(memory,state,b,1,0,trailing,address,length)) return StepResult::memory_fault;
            if (op32==0x8d) *state.reg(reg)=static_cast<std::uint32_t>(address);
            else if (op32==0x89 || op32==0xc7) {
                const auto value=op32==0xc7 ? static_cast<std::uint32_t>(little(b.data()+length-4,4)) : static_cast<std::uint32_t>(*state.reg(reg));
                std::array<std::uint8_t,4> data{};
                for (unsigned i=0;i<4;++i) data[i]=static_cast<std::uint8_t>(value>>(8*i));
                if (!memory.write(address,data)) return StepResult::memory_fault;
            } else {
                std::array<std::uint8_t,4> data{};
                if (!memory.read(address,data)) return StepResult::memory_fault;
                const auto value=static_cast<std::uint32_t>(little(data.data(),4));
                if (op32==0x8b) *state.reg(reg)=value;
                else if (op32==0x85) logical_test32(state,value & static_cast<std::uint32_t>(*state.reg(reg)));
                else {
                    const bool to_reg=op32==0x03 || op32==0x2b || op32==0x33 || op32==0x0b || op32==0x23 || op32==0x3b;
                    const bool compare=op32==0x39 || op32==0x3b || ((op32==0x81 || op32==0x83) && reg==7);
                    const bool subtract=op32==0x29 || op32==0x2b || compare || ((op32==0x81 || op32==0x83) && reg==5);
                    const auto rhs=op32==0x81 ? static_cast<std::uint32_t>(little(b.data()+length-4,4)) :
                                   op32==0x83 ? static_cast<std::uint32_t>(sign_extend(b[length-1],8)) :
                                   to_reg ? value : static_cast<std::uint32_t>(*state.reg(reg));
                    // Stage flags and result so a failed write does not change CPU state.
                    auto next=state;
                    std::uint64_t dst=to_reg ? *state.reg(reg) : value;
                    if ((op32==0x81 || op32==0x83) && group_logical(reg)) group_logical32(next,dst,rhs,reg);
                    else if (op32==0x31 || op32==0x33 || op32==0x09 || op32==0x0b || op32==0x21 || op32==0x23) {
                        dst=static_cast<std::uint32_t>(dst);
                        if (op32==0x31 || op32==0x33) dst^=rhs;
                        else if (op32==0x09 || op32==0x0b) dst|=rhs;
                        else dst&=rhs;
                        logical_test32(next,static_cast<std::uint32_t>(dst));
                    } else arithmetic32(next,dst,rhs,subtract,compare);
                    if (!to_reg && !compare) {
                        for (unsigned i=0;i<4;++i) data[i]=static_cast<std::uint8_t>(dst>>(8*i));
                        if (!memory.write(address,data)) return StepResult::memory_fault;
                    }
                    if (to_reg && !compare) *next.reg(reg)=dst;
                    state=next;
                }
            }
            state.rip+=length; return StepResult::running;
        }
        if (op32==0x8d) return StepResult::invalid_instruction;
        if (op32==0xc7) {
            if (reg!=0) return StepResult::invalid_instruction;
            if (!memory.fetch(state.rip,std::span(b).first(6))) return StepResult::memory_fault;
            *state.reg(rm)=little(b.data()+2,4); state.rip+=6; return StepResult::running;
        }
        if (op32==0x81 || op32==0x83) {
            if (reg!=0 && reg!=5 && reg!=7 && !group_logical(reg)) return StepResult::invalid_instruction;
            const unsigned width=op32==0x81?4:1, length=2+width;
            if (!memory.fetch(state.rip,std::span(b).first(length))) return StepResult::memory_fault;
            const auto imm=little(b.data()+2,width);
            const auto rhs=static_cast<std::uint32_t>(width==1?sign_extend(imm,8):imm);
            if (group_logical(reg)) group_logical32(state,*state.reg(rm),rhs,reg);
            else arithmetic32(state,*state.reg(rm),rhs,reg!=0,reg==7);
            state.rip+=length; return StepResult::running;
        }
        if (op32==0x85) {
            logical_test32(state,static_cast<std::uint32_t>(*state.reg(reg)) & static_cast<std::uint32_t>(*state.reg(rm)));
            state.rip+=2; return StepResult::running;
        }
        const bool to_reg=op32==0x8b || op32==0x03 || op32==0x2b || op32==0x33 || op32==0x0b || op32==0x23 || op32==0x3b;
        auto& dst=*state.reg(to_reg?reg:rm);
        const auto src=static_cast<std::uint32_t>(*state.reg(to_reg?rm:reg));
        if (op32==0x89 || op32==0x8b) dst=src;
        else if (op32==0x31 || op32==0x33 || op32==0x09 || op32==0x0b || op32==0x21 || op32==0x23) {
            dst=static_cast<std::uint32_t>(dst);
            if (op32==0x31 || op32==0x33) dst^=src;
            else if (op32==0x09 || op32==0x0b) dst|=src;
            else dst&=src;
            logical_test32(state,static_cast<std::uint32_t>(dst));
        }
        else arithmetic32(state,dst,src,op32==0x29 || op32==0x2b || op32==0x39 || op32==0x3b,
                          op32==0x39 || op32==0x3b);
        state.rip+=2; return StepResult::running;
    }
    // Supported REX.W prefixes with optional R and B extensions.
    if (b[0] < 0x48 || b[0] > 0x4f)
        return StepResult::invalid_instruction;
    if (!memory.fetch(state.rip, std::span(b).first(2))) return StepResult::memory_fault;
    if ((b[0] == 0x48 || b[0] == 0x49) && b[1] >= 0xb8 && b[1] <= 0xbf) {
        if (!memory.fetch(state.rip, std::span(b).first(10))) return StepResult::memory_fault;
        const unsigned index=(b[1]-0xb8) + ((b[0] & 1) ? 8 : 0);
        *state.reg(index) = little(b.data()+2, 8);
        state.rip += 10;
        return StepResult::running;
    }
    if (b[0] == 0x48 && (b[1] == 0x05 || b[1] == 0x2d || b[1] == 0x3d)) {
        if (!memory.fetch(state.rip, std::span(b).first(6))) return StepResult::memory_fault;
        auto immediate = little(b.data() + 2, 4);
        const std::uint64_t rhs = (immediate & 0x80000000ULL) ? immediate | 0xffffffff00000000ULL : immediate;
        arithmetic(state,state.rax,rhs,b[1]!=0x05,b[1]==0x3d);
        state.rip += 6;
        return StepResult::running;
    }
    const auto opcode=b[1];
    if (opcode==0xc7) { // MOV r/m64, sign-extended imm32
        if (!memory.fetch(state.rip,std::span(b).first(3))) return StepResult::memory_fault;
        if ((b[2]&0x38)!=0) return StepResult::invalid_instruction;
        if ((b[2]>>6)==3) {
            if (!memory.fetch(state.rip,std::span(b).first(7))) return StepResult::memory_fault;
            *state.reg((b[2]&7)+((b[0]&1)?8:0))=sign_extend(little(b.data()+3,4),32);
            state.rip+=7; return StepResult::running;
        }
        std::uint64_t address=0; unsigned length=0;
        if (!effective_address(memory,state,b,2,b[0],4,address,length)) return StepResult::memory_fault;
        if (!memory.write(address,bytes64(sign_extend(little(b.data()+length-4,4),32)))) return StepResult::memory_fault;
        state.rip+=length; return StepResult::running;
    }
    if (opcode==0x81 || opcode==0x83) { // REX.W group 1, sign-extended immediate
        if (!memory.fetch(state.rip,std::span(b).first(3))) return StepResult::memory_fault;
        const auto mod=b[2]>>6, extension=(b[2]>>3)&7;
        if (extension!=0 && extension!=5 && extension!=7 && !group_logical(extension)) return StepResult::invalid_instruction;
        const unsigned width=opcode==0x81?4:1;
        if (mod!=3) {
            std::uint64_t address=0; unsigned length=0;
            if (!effective_address(memory,state,b,2,b[0],width,address,length)) return StepResult::memory_fault;
            std::array<std::uint8_t,8> bytes{};
            if (!memory.read(address,bytes)) return StepResult::memory_fault;
            auto next=state; auto value=little(bytes.data(),8);
            const auto rhs=sign_extend(little(b.data()+length-width,width),width*8);
            if (group_logical(extension)) group_logical64(next,value,rhs,extension);
            else arithmetic(next,value,rhs,extension!=0,extension==7);
            if (extension!=7 && !memory.write(address,bytes64(value))) return StepResult::memory_fault;
            state=next; state.rip+=length; return StepResult::running;
        }
        const unsigned length=3+width;
        if (!memory.fetch(state.rip,std::span(b).first(length))) return StepResult::memory_fault;
        const unsigned reg=(b[2]&7)+((b[0]&1)?8:0);
        const auto rhs=sign_extend(little(b.data()+3,width),width*8);
        if (group_logical(extension)) group_logical64(state,*state.reg(reg),rhs,extension);
        else arithmetic(state,*state.reg(reg),rhs,extension!=0,extension==7);
        state.rip+=length;
        return StepResult::running;
    }
    if (opcode==0x89 || opcode==0x8b || opcode==0x8d || opcode==0x01 || opcode==0x03 ||
        opcode==0x29 || opcode==0x2b || opcode==0x31 || opcode==0x33 || opcode==0x09 || opcode==0x0b || opcode==0x21 || opcode==0x23 || opcode==0x39 || opcode==0x3b || opcode==0x85) {
        if (!memory.fetch(state.rip, std::span(b).first(3))) return StepResult::memory_fault;
        const unsigned mod=b[2]>>6;
        const unsigned reg=((b[2]>>3)&7) + ((b[0]&4) ? 8 : 0);
        const unsigned low_rm=b[2]&7;
        const unsigned rm=low_rm + ((b[0]&1) ? 8 : 0);
        if (mod!=3) {
            // Base/SIB/RIP-relative data operands, with staged fault behavior.
            unsigned length=0; std::uint64_t address=0;
            if (!effective_address(memory,state,b,2,b[0],0,address,length)) return StepResult::memory_fault;
            if (opcode==0x8d) { // LEA computes an address without accessing it.
                *state.reg(reg)=address; state.rip+=length; return StepResult::running;
            }
            if (opcode==0x89) {
                if (!memory.write(address,bytes64(*state.reg(reg)))) return StepResult::memory_fault;
            } else {
                std::array<std::uint8_t,8> bytes{};
                if (!memory.read(address,bytes)) return StepResult::memory_fault;
                const auto value=little(bytes.data(),8);
                if (opcode==0x8b) *state.reg(reg)=value;
                else if (opcode==0x85) logical_test(state,value & *state.reg(reg));
                else {
                    const bool to_reg=opcode==0x03 || opcode==0x2b || opcode==0x33 || opcode==0x0b || opcode==0x23 || opcode==0x3b;
                    const bool compare=opcode==0x39 || opcode==0x3b;
                    const auto rhs=to_reg ? value : *state.reg(reg);
                    auto next=state; auto dst=to_reg ? *state.reg(reg) : value;
                    if (opcode==0x31 || opcode==0x33 || opcode==0x09 || opcode==0x0b || opcode==0x21 || opcode==0x23) {
                        if (opcode==0x31 || opcode==0x33) dst^=rhs;
                        else if (opcode==0x09 || opcode==0x0b) dst|=rhs;
                        else dst&=rhs;
                        logical_test(next,dst);
                    }
                    else arithmetic(next,dst,rhs,opcode==0x29 || opcode==0x2b || compare,compare);
                    if (!to_reg && !compare && !memory.write(address,bytes64(dst))) return StepResult::memory_fault;
                    if (to_reg && !compare) *next.reg(reg)=dst;
                    state=next;
                }
            }
            state.rip+=length;
            return StepResult::running;
        }
        if (opcode==0x8d) return StepResult::invalid_instruction; // LEA requires memory form
        if (opcode==0x85) {
            logical_test(state,*state.reg(reg) & *state.reg(rm));
            state.rip+=3; return StepResult::running;
        }
        const bool to_reg=opcode==0x8b || opcode==0x03 || opcode==0x2b || opcode==0x33 || opcode==0x0b || opcode==0x23 || opcode==0x3b;
        auto& dst=*state.reg(to_reg ? reg : rm);
        const auto src=*state.reg(to_reg ? rm : reg);
        if (opcode==0x89 || opcode==0x8b) dst=src;
        else if (opcode==0x31 || opcode==0x33 || opcode==0x09 || opcode==0x0b || opcode==0x21 || opcode==0x23) {
            if (opcode==0x31 || opcode==0x33) dst^=src;
            else if (opcode==0x09 || opcode==0x0b) dst|=src;
            else dst&=src;
            logical_test(state,dst);
        }
        else arithmetic(state,dst,src,opcode==0x29 || opcode==0x2b || opcode==0x39 || opcode==0x3b,
                        opcode==0x39 || opcode==0x3b);
        state.rip += 3;
        return StepResult::running;
    }
    return StepResult::invalid_instruction;
}
}

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
// Portable upper half of a 64x64 unsigned product using 32-bit limbs.
std::uint64_t multiply_high64(std::uint64_t a,std::uint64_t b) {
    const auto a0=std::uint64_t{static_cast<std::uint32_t>(a)}, a1=a>>32;
    const auto b0=std::uint64_t{static_cast<std::uint32_t>(b)}, b1=b>>32;
    const auto p00=a0*b0, p01=a0*b1, p10=a1*b0, p11=a1*b1;
    const auto middle=(p00>>32)+static_cast<std::uint32_t>(p01)+static_cast<std::uint32_t>(p10);
    return p11+(p01>>32)+(p10>>32)+(middle>>32);
}
// Signed multiplication using unsigned arithmetic, avoiding signed overflow and
// compiler-specific 128-bit types (including in Windows/MSVC builds).
std::uint64_t imul_value(CpuState& state,std::uint64_t a,std::uint64_t b,unsigned width) {
    const auto mask=width==16 ? 0xffffULL : width==32 ? 0xffffffffULL : ~std::uint64_t{0};
    const auto sign=std::uint64_t{1}<<(width-1);
    a&=mask; b&=mask;
    const bool negative=bool(a&sign)!=bool(b&sign);
    const auto mag_a=(a&sign)?(std::uint64_t{0}-a)&mask:a;
    const auto mag_b=(b&sign)?(std::uint64_t{0}-b)&mask:b;
    const auto limit=negative?sign:sign-1;
    const bool overflow=mag_b && mag_a>limit/mag_b;
    constexpr std::uint64_t flags=(1ULL<<0)|(1ULL<<11);
    state.rflags=(state.rflags&~flags)|(overflow?flags:0); // undefined flags preserved
    return (a*b)&mask;
}
// SHL/SHR/SAR: undefined AF and OF for counts greater than one are
// preserved in this model. Counts have already been masked to 5/6 bits.
std::uint64_t shift_value(CpuState& state, std::uint64_t operand, unsigned width, unsigned extension, unsigned count) {
    const auto mask=width==32 ? 0xffffffffULL : ~std::uint64_t{0};
    const auto lhs=operand & mask;
    if (!count) return lhs;
    std::uint64_t result=0;
    bool carry=false;
    if (extension==4) { carry=(lhs>>(width-count))&1; result=(lhs<<count)&mask; }
    else {
        carry=(lhs>>(count-1))&1; result=lhs>>count;
        if (extension==7 && ((lhs>>(width-1))&1)) result|=mask ^ (mask>>count);
    }
    constexpr std::uint64_t CF=1ULL, PF=1ULL<<2, ZF=1ULL<<6, SF=1ULL<<7, OF=1ULL<<11;
    std::uint64_t flags=carry?CF:0;
    if ((std::popcount(static_cast<unsigned>(result&0xff))&1)==0) flags|=PF;
    if (!result) flags|=ZF;
    if ((result>>(width-1))&1) flags|=SF;
    auto update=CF|PF|ZF|SF;
    if (count==1) {
        update|=OF;
        if (extension==4 ? bool((result>>(width-1))&1)!=carry : extension==5 && ((lhs>>(width-1))&1)) flags|=OF;
    }
    state.rflags=(state.rflags&~update)|flags;
    return result;
}
}
StepResult Cpu::step(Memory& memory) {
    std::array<std::uint8_t, 15> b{};
    if (!memory.fetch(state.rip, std::span(b).first(1))) return StepResult::memory_fault;
    if (b[0] == 0x90 || b[0] == 0xf4) {
        ++state.rip;
        return b[0] == 0xf4 ? StepResult::halted : StepResult::running;
    }
    // CBW, CWDE and CDQE: sign-extend accumulator into the next size.
    if (b[0]==0x98) { state.rax=static_cast<std::uint32_t>(sign_extend(state.rax&0xffff,16)); ++state.rip; return StepResult::running; }
    if (b[0]==0x66 || b[0]==0x48) {
        if (!memory.fetch(state.rip,std::span(b).first(2))) return StepResult::memory_fault;
        if (b[1]==0x98) {
            if (b[0]==0x66) state.rax=(state.rax&~0xffffULL)|(sign_extend(state.rax&0xff,8)&0xffff);
            else state.rax=sign_extend(state.rax&0xffffffffULL,32);
            state.rip+=2; return StepResult::running;
        }
    }
    // Sign-extension of accumulator into the data:accumulator pair.
    // Only the accumulator and data registers are touched; flags are unchanged.
    if (b[0]==0x99) { *state.reg(2)=(state.rax&0x80000000ULL)?0xffffffffULL:0; ++state.rip; return StepResult::running; }
    if (b[0]==0x48) {
        if (!memory.fetch(state.rip,std::span(b).first(2))) return StepResult::memory_fault;
        if (b[1]==0x99) { *state.reg(2)=(state.rax>>63)?~std::uint64_t{0}:0; state.rip+=2; return StepResult::running; }
    }
    if (b[0]==0x66) {
        if (!memory.fetch(state.rip,std::span(b).first(2))) return StepResult::memory_fault;
        if (b[1]==0x99) { auto& rdx=*state.reg(2); rdx=(rdx&~0xffffULL)|((state.rax&0x8000)?0xffffULL:0); state.rip+=2; return StepResult::running; }
    }
    // Byte MUL/IMUL: AL times r/m8, full product in AX. REX changes
    // encodings 4..7 from AH..BH to SPL..DIL.
    if (b[0]==0xf6 || (b[0]>=0x40 && b[0]<=0x4f)) {
        const bool has_rex=b[0]!=0xf6; const auto rex=has_rex?b[0]:std::uint8_t{0};
        const unsigned op_pos=has_rex?1:0;
        if (has_rex && !memory.fetch(state.rip,std::span(b).first(2))) return StepResult::memory_fault;
        if (b[op_pos]==0xf6) {
            const auto modrm_pos=op_pos+1;
            if (!memory.fetch(state.rip,std::span(b).first(modrm_pos+1))) return StepResult::memory_fault;
            const unsigned ext=(b[modrm_pos]>>3)&7;
            if (ext==4 || ext==5) {
                const unsigned mod=b[modrm_pos]>>6, low_rm=b[modrm_pos]&7;
                unsigned length=modrm_pos+1; std::uint64_t source=0;
                if(mod==3) source=(!has_rex && low_rm>=4)?(*state.reg(low_rm-4)>>8)&0xff:*state.reg(low_rm+((rex&1)?8:0))&0xff;
                else {
                    std::uint64_t address=0; std::array<std::uint8_t,1> value{};
                    if (!effective_address(memory,state,b,modrm_pos,rex,0,address,length) || !memory.read(address,value)) return StepResult::memory_fault;
                    source=value[0];
                }
                const auto lhs=state.rax&0xff;
                const auto low=(lhs*source)&0xff;
                auto high=((lhs*source)>>8)&0xff;
                if(ext==5) { if(lhs&0x80) high=(high-source)&0xff; if(source&0x80) high=(high-lhs)&0xff; }
                const bool overflow=ext==4?high!=0:high!=((low&0x80)?0xff:0);
                state.rax=(state.rax&~0xffffULL)|(high<<8)|low;
                constexpr auto flags=(1ULL<<0)|(1ULL<<11);
                state.rflags=(state.rflags&~flags)|(overflow?flags:0);
                state.rip+=length; return StepResult::running;
            }
        }
    }
    // One-operand MUL/IMUL: full-width product in DX:AX, EDX:EAX or
    // RDX:RAX. Undefined flags are preserved; no host-specific 128-bit type.
    if (b[0]==0xf7 || b[0]==0x66 || (b[0]>=0x40 && b[0]<=0x4f)) {
        const bool word=b[0]==0x66; unsigned pos=word?1:0;
        if (word && !memory.fetch(state.rip,std::span(b).first(2))) return StepResult::memory_fault;
        const bool has_rex=b[pos]>=0x40 && b[pos]<=0x4f;
        const auto rex=has_rex?b[pos]:std::uint8_t{0};
        if (has_rex) { ++pos; if (!memory.fetch(state.rip,std::span(b).first(pos+1))) return StepResult::memory_fault; }
        if (b[pos]==0xf7) {
            const unsigned modrm_pos=pos+1;
            if (!memory.fetch(state.rip,std::span(b).first(modrm_pos+1))) return StepResult::memory_fault;
            const unsigned ext=(b[modrm_pos]>>3)&7;
            if (ext==4 || ext==5) {
                if (word && (rex&8)) return StepResult::invalid_instruction;
                const unsigned width=word?16:(rex&8)?64:32;
                const auto mask=word?0xffffULL:width==32?0xffffffffULL:~std::uint64_t{0};
                const auto sign=std::uint64_t{1}<<(width-1);
                const unsigned mod=b[modrm_pos]>>6, rm=(b[modrm_pos]&7)+((rex&1)?8:0);
                std::uint64_t source=0; unsigned length=modrm_pos+1;
                if(mod==3) source=*state.reg(rm)&mask;
                else {
                    std::uint64_t address=0;
                    if (!effective_address(memory,state,b,modrm_pos,rex,0,address,length)) return StepResult::memory_fault;
                    std::array<std::uint8_t,8> value{};
                    if (!memory.read(address,std::span(value).first(width/8))) return StepResult::memory_fault;
                    source=little(value.data(),width/8);
                }
                const auto lhs=state.rax&mask, low=(lhs*source)&mask;
                auto high=width==64?multiply_high64(lhs,source):((lhs*source)>>width)&mask;
                if (ext==5) {
                    if(lhs&sign) high=(high-source)&mask;
                    if(source&sign) high=(high-lhs)&mask;
                }
                const bool overflow=ext==4?high!=0:high!=((low&sign)?mask:0);
                auto& rdx=*state.reg(2);
                if(word) { state.rax=(state.rax&~mask)|low; rdx=(rdx&~mask)|high; }
                else { state.rax=low; rdx=high; }
                constexpr auto flags=(1ULL<<0)|(1ULL<<11);
                state.rflags=(state.rflags&~flags)|(overflow?flags:0);
                state.rip+=length; return StepResult::running;
            }
        }
    }
    // IMUL r16/r32/r64, r/m and the 69/6B immediate forms. Memory and
    // instruction bytes are read before committing the register or flags.
    if (b[0]==0x0f || b[0]==0x69 || b[0]==0x6b || b[0]==0x66 || (b[0]>=0x40 && b[0]<=0x4f)) {
        const bool word=b[0]==0x66;
        unsigned op_pos=word?1:0;
        if (word && !memory.fetch(state.rip,std::span(b).first(2))) return StepResult::memory_fault;
        const bool has_rex=b[op_pos]>=0x40 && b[op_pos]<=0x4f;
        const auto rex=has_rex?b[op_pos]:std::uint8_t{0};
        if (has_rex) { ++op_pos; if (!memory.fetch(state.rip,std::span(b).first(op_pos+1))) return StepResult::memory_fault; }
        const auto op=b[op_pos];
        bool imul=op==0x69 || op==0x6b;
        if (op==0x0f) {
            if (!memory.fetch(state.rip,std::span(b).first(op_pos+2))) return StepResult::memory_fault;
            imul=b[op_pos+1]==0xaf;
        }
        if (imul) {
            if (word && (rex&8)) return StepResult::invalid_instruction;
            const unsigned width=word?16:(rex&8)?64:32;
            const unsigned immediate_size=op==0x69?(word?2:4):op==0x6b?1:0;
            const unsigned modrm_pos=op_pos+(op==0x0f?2:1);
            if (!memory.fetch(state.rip,std::span(b).first(modrm_pos+1))) return StepResult::memory_fault;
            const unsigned mod=b[modrm_pos]>>6;
            const unsigned reg=((b[modrm_pos]>>3)&7)+((rex&4)?8:0);
            const unsigned rm=(b[modrm_pos]&7)+((rex&1)?8:0);
            unsigned length=modrm_pos+1; std::uint64_t source=0;
            if (mod==3) {
                length+=immediate_size;
                if (immediate_size && !memory.fetch(state.rip,std::span(b).first(length))) return StepResult::memory_fault;
                source=*state.reg(rm);
            } else {
                std::uint64_t address=0;
                if (!effective_address(memory,state,b,modrm_pos,rex,immediate_size,address,length)) return StepResult::memory_fault;
                std::array<std::uint8_t,8> value{};
                if (!memory.read(address,std::span(value).first(width/8))) return StepResult::memory_fault;
                source=little(value.data(),width/8);
            }
            const auto rhs=immediate_size?sign_extend(little(b.data()+length-immediate_size,immediate_size),immediate_size*8):*state.reg(reg);
            const auto result=imul_value(state,source,rhs,width);
            auto& destination=*state.reg(reg);
            destination=word?(destination&~0xffffULL)|result:result;
            state.rip+=length; return StepResult::running;
        }
    }
    // Operand-size override MOV: word register/memory/immediate forms.
    // Only one optional REX prefix after 66 is supported. REX.W overrides
    // 66 in x86; that combination is deliberately rejected here.
    if (b[0]==0x66) {
        if (!memory.fetch(state.rip,std::span(b).first(2))) return StepResult::memory_fault;
        const bool has_rex=b[1]>=0x40 && b[1]<=0x4f;
        const auto rex=has_rex?b[1]:std::uint8_t{0}; const unsigned op_pos=has_rex?2:1;
        if (has_rex && !memory.fetch(state.rip,std::span(b).first(3))) return StepResult::memory_fault;
        if (rex&8) return StepResult::invalid_instruction;
        const auto op=b[op_pos];
        auto write_word=[&](unsigned index,std::uint16_t value) {
            auto& target=*state.reg(index); target=(target&~std::uint64_t{0xffff})|value;
        };
        if (op>=0xb8 && op<=0xbf) {
            const unsigned length=op_pos+3;
            if (!memory.fetch(state.rip,std::span(b).first(length))) return StepResult::memory_fault;
            write_word(op-0xb8+((rex&1)?8:0),static_cast<std::uint16_t>(little(b.data()+op_pos+1,2)));
            state.rip+=length; return StepResult::running;
        }
        if (op==0x89 || op==0x8b || op==0xc7) {
            const unsigned modrm_pos=op_pos+1;
            if (!memory.fetch(state.rip,std::span(b).first(modrm_pos+1))) return StepResult::memory_fault;
            const unsigned mod=b[modrm_pos]>>6, low_reg=(b[modrm_pos]>>3)&7;
            const unsigned reg=low_reg+((rex&4)?8:0), rm=(b[modrm_pos]&7)+((rex&1)?8:0);
            if (op==0xc7 && low_reg!=0) return StepResult::invalid_instruction;
            unsigned length=modrm_pos+1; const unsigned trailing=op==0xc7?2:0;
            if (mod==3) {
                length+=trailing;
                if (trailing && !memory.fetch(state.rip,std::span(b).first(length))) return StepResult::memory_fault;
                const auto value=op==0xc7?static_cast<std::uint16_t>(little(b.data()+length-2,2)):
                                 static_cast<std::uint16_t>(*state.reg(op==0x8b?rm:reg));
                write_word(op==0x8b?reg:rm,value);
            } else {
                std::uint64_t address=0;
                if (!effective_address(memory,state,b,modrm_pos,rex,trailing,address,length)) return StepResult::memory_fault;
                if (op==0x8b) {
                    std::array<std::uint8_t,2> data{}; if (!memory.read(address,data)) return StepResult::memory_fault;
                    write_word(reg,static_cast<std::uint16_t>(little(data.data(),2)));
                } else {
                    auto value=op==0xc7?static_cast<std::uint16_t>(little(b.data()+length-2,2)):static_cast<std::uint16_t>(*state.reg(reg));
                    const std::array<std::uint8_t,2> data{static_cast<std::uint8_t>(value),static_cast<std::uint8_t>(value>>8)};
                    if (!memory.write(address,data)) return StepResult::memory_fault;
                }
            }
            state.rip+=length; return StepResult::running;
        }
        return StepResult::invalid_instruction;
    }
    // MOVZX/MOVSX r32/r64, r/m8 or r/m16, and MOVSXD r64, r/m32.
    // Any REX prefix replaces AH..BH with SPL..DIL for byte sources.
    if (b[0]==0x0f || (b[0]>=0x40 && b[0]<=0x4f)) {
        const bool has_rex=b[0]!=0x0f;
        const unsigned op_pos=has_rex?1:0;
        if (has_rex && !memory.fetch(state.rip,std::span(b).first(2))) return StepResult::memory_fault;
        const std::uint8_t rex=has_rex?b[0]:0;
        unsigned op=0, modrm_pos=0, width=0;
        bool is_signed=false;
        if (b[op_pos]==0x0f) {
            if (!memory.fetch(state.rip,std::span(b).first(op_pos+2))) return StepResult::memory_fault;
            op=b[op_pos+1]; modrm_pos=op_pos+2;
            if (op==0xb6 || op==0xbe) width=1;
            else if (op==0xb7 || op==0xbf) width=2;
            is_signed=op==0xbe || op==0xbf;
        } else if (has_rex && (rex&8) && b[op_pos]==0x63) {
            width=4; is_signed=true; modrm_pos=op_pos+1;
        }
        if (width) {
            if (!memory.fetch(state.rip,std::span(b).first(modrm_pos+1))) return StepResult::memory_fault;
            const unsigned mod=b[modrm_pos]>>6, low_rm=b[modrm_pos]&7;
            const unsigned reg=((b[modrm_pos]>>3)&7)+((rex&4)?8:0);
            unsigned length=modrm_pos+1; std::uint64_t value=0;
            if (mod==3) {
                if (width==1 && !has_rex && low_rm>=4)
                    value=(*state.reg(low_rm-4)>>8)&0xff; // AH/CH/DH/BH
                else {
                    value=*state.reg(low_rm+((rex&1)?8:0));
                    value &= width==1 ? 0xffULL : width==2 ? 0xffffULL : 0xffffffffULL;
                }
            } else {
                std::uint64_t address=0;
                if (!effective_address(memory,state,b,modrm_pos,rex,0,address,length)) return StepResult::memory_fault;
                std::array<std::uint8_t,4> data{};
                if (!memory.read(address,std::span(data).first(width))) return StepResult::memory_fault;
                value=little(data.data(),width);
            }
            if (is_signed) value=sign_extend(value,width*8);
            if (!(rex&8)) value=static_cast<std::uint32_t>(value); // r32 writes zero-extend.
            *state.reg(reg)=value; state.rip+=length;
            return StepResult::running;
        }
    }
    // Byte MOV between registers/memory, and MOV r/m8, imm8 (C6 /0).
    // Legacy high-byte registers are available only without any REX prefix.
    if (b[0]==0x88 || b[0]==0x8a || b[0]==0xc6 || (b[0]>=0x40 && b[0]<=0x4f)) {
        const bool has_rex=b[0]>=0x40 && b[0]<=0x4f;
        const auto rex=has_rex?b[0]:std::uint8_t{0}; const unsigned op_pos=has_rex?1:0;
        if (has_rex && !memory.fetch(state.rip,std::span(b).first(2))) return StepResult::memory_fault;
        const auto op=b[op_pos];
        if (op==0x88 || op==0x8a || op==0xc6) {
            const unsigned modrm_pos=op_pos+1;
            if (!memory.fetch(state.rip,std::span(b).first(modrm_pos+1))) return StepResult::memory_fault;
            const unsigned mod=b[modrm_pos]>>6, low_rm=b[modrm_pos]&7, low_reg=(b[modrm_pos]>>3)&7;
            if (op==0xc6 && low_reg!=0) return StepResult::invalid_instruction;
            auto read_byte_reg=[&](unsigned low, bool extended) -> std::uint8_t {
                if (!has_rex && low>=4) return static_cast<std::uint8_t>(*state.reg(low-4)>>8);
                return static_cast<std::uint8_t>(*state.reg(low+(extended?8:0)));
            };
            auto write_byte_reg=[&](unsigned low, bool extended, std::uint8_t value) {
                const bool high=!has_rex && low>=4;
                auto& target=*state.reg(high?low-4:low+(extended?8:0));
                const unsigned shift=high?8:0;
                target=(target & ~(std::uint64_t{0xff}<<shift)) | (std::uint64_t(value)<<shift);
            };
            unsigned length=modrm_pos+1; const unsigned trailing=op==0xc6?1:0;
            if (mod==3) {
                length+=trailing;
                if (trailing && !memory.fetch(state.rip,std::span(b).first(length))) return StepResult::memory_fault;
                const auto value=op==0xc6?b[length-1]:op==0x88?read_byte_reg(low_reg,rex&4):read_byte_reg(low_rm,rex&1);
                if (op==0x8a) write_byte_reg(low_reg,rex&4,value);
                else write_byte_reg(low_rm,rex&1,value);
            } else {
                std::uint64_t address=0;
                if (!effective_address(memory,state,b,modrm_pos,rex,trailing,address,length)) return StepResult::memory_fault;
                if (op==0x8a) {
                    std::array<std::uint8_t,1> data{};
                    if (!memory.read(address,data)) return StepResult::memory_fault;
                    write_byte_reg(low_reg,rex&4,data[0]);
                } else {
                    const std::array<std::uint8_t,1> data{op==0xc6?b[length-1]:read_byte_reg(low_reg,rex&4)};
                    if (!memory.write(address,data)) return StepResult::memory_fault;
                }
            }
            state.rip+=length; return StepResult::running;
        }
    }
    // F7 /2 NOT and /3 NEG on r/m32 or r/m64. NOT preserves all flags.
    if (b[0]==0xf7 || (b[0]>=0x40 && b[0]<=0x4f)) {
        const bool has_rex=b[0]>=0x40 && b[0]<=0x4f;
        const auto rex=has_rex?b[0]:std::uint8_t{0}; const unsigned op_pos=has_rex?1:0;
        if (has_rex && !memory.fetch(state.rip,std::span(b).first(2))) return StepResult::memory_fault;
        if (b[op_pos]==0xf7) {
            const unsigned modrm_pos=op_pos+1;
            if (!memory.fetch(state.rip,std::span(b).first(modrm_pos+1))) return StepResult::memory_fault;
            const unsigned mod=b[modrm_pos]>>6, ext=(b[modrm_pos]>>3)&7, rm=(b[modrm_pos]&7)+((rex&1)?8:0);
            if (ext!=2 && ext!=3) return StepResult::invalid_instruction;
            const unsigned width=(rex&8)?64:32; unsigned length=modrm_pos+1; std::uint64_t address=0, value=0;
            if (mod==3) value=*state.reg(rm);
            else {
                if (!effective_address(memory,state,b,modrm_pos,rex,0,address,length)) return StepResult::memory_fault;
                std::array<std::uint8_t,8> bytes{};
                if (!memory.read(address,std::span(bytes).first(width/8))) return StepResult::memory_fault;
                value=little(bytes.data(),width/8);
            }
            auto next=state;
            if (ext==2) value=width==32 ? std::uint32_t(~value) : ~value;
            else {
                const auto operand=value; value=0;
                if (width==32) arithmetic32(next,value,static_cast<std::uint32_t>(operand),true,false);
                else arithmetic(next,value,operand,true,false);
            }
            if (mod==3) *next.reg(rm)=value;
            else { const auto bytes=bytes64(value); if (!memory.write(address,std::span(bytes).first(width/8))) return StepResult::memory_fault; }
            state=next; state.rip+=length; return StepResult::running;
        }
    }
    // Group-2 SHL/SHR/SAR r/m32,r/m64, counts 1, CL or immediate byte.
    if (b[0]==0xd1 || b[0]==0xd3 || b[0]==0xc1 || (b[0]>=0x40 && b[0]<=0x4f)) {
        const bool has_rex=b[0]>=0x40 && b[0]<=0x4f;
        const auto rex=has_rex?b[0]:std::uint8_t{0}; const unsigned op_pos=has_rex?1:0;
        if (has_rex && !memory.fetch(state.rip,std::span(b).first(2))) return StepResult::memory_fault;
        const auto op=b[op_pos];
        if (op==0xd1 || op==0xd3 || op==0xc1) {
            const unsigned modrm_pos=op_pos+1;
            if (!memory.fetch(state.rip,std::span(b).first(modrm_pos+1))) return StepResult::memory_fault;
            const unsigned mod=b[modrm_pos]>>6, extension=(b[modrm_pos]>>3)&7, rm=b[modrm_pos]&7;
            if (extension!=4 && extension!=5 && extension!=7) return StepResult::invalid_instruction;
            const unsigned width=(rex&8)?64:32, trailing=op==0xc1?1:0;
            unsigned length=modrm_pos+1; std::uint64_t address=0, value=0;
            if (mod==3) {
                length+=trailing;
                if (trailing && !memory.fetch(state.rip,std::span(b).first(length))) return StepResult::memory_fault;
                value=*state.reg(rm+((rex&1)?8:0));
            } else {
                if (!effective_address(memory,state,b,modrm_pos,rex,trailing,address,length)) return StepResult::memory_fault;
                std::array<std::uint8_t,8> data{};
                if (!memory.read(address,std::span(data).first(width/8))) return StepResult::memory_fault;
                value=little(data.data(),width/8);
            }
            // CL is read before any destination register is updated, even when it aliases RCX.
            const unsigned raw_count=op==0xd1?1:op==0xd3?static_cast<unsigned>(*state.reg(1)&0xff):b[length-1];
            const unsigned count=raw_count & (width==64?63:31);
            auto next=state; const auto result=shift_value(next,value,width,extension,count);
            if (mod==3) *next.reg(rm+((rex&1)?8:0))=result;
            else {
                const auto data=bytes64(result);
                if (!memory.write(address,std::span(data).first(width/8))) return StepResult::memory_fault;
            }
            state=next; state.rip+=length; return StepResult::running;
        }
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
            const std::uint8_t rex=rex_indirect?b[0]:0;
            // FF /0 and /1: INC/DEC r/m32 or REX.W r/m64. CF is preserved.
            if (extension==0 || extension==1) {
                const unsigned width=(rex&8)?64:32, rm=(b[modrm_pos]&7)+((rex&1)?8:0);
                unsigned length=modrm_pos+1; std::uint64_t address=0, value=0;
                if (mod==3) value=*state.reg(rm);
                else {
                    if (!effective_address(memory,state,b,modrm_pos,rex,0,address,length)) return StepResult::memory_fault;
                    std::array<std::uint8_t,8> bytes{};
                    if (!memory.read(address,std::span(bytes).first(width/8))) return StepResult::memory_fault;
                    value=little(bytes.data(),width/8);
                }
                auto next=state;
                if (width==64) arithmetic(next,value,1,extension==1,false);
                else arithmetic32(next,value,1,extension==1,false);
                next.rflags=(next.rflags&~std::uint64_t{1})|(state.rflags&1);
                if (mod==3) *next.reg(rm)=value;
                else { const auto bytes=bytes64(value); if (!memory.write(address,std::span(bytes).first(width/8))) return StepResult::memory_fault; }
                state=next; state.rip+=length; return StepResult::running;
            }
            if (extension!=2 && extension!=4 && extension!=6) return StepResult::invalid_instruction;
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
    // Accumulator logical operations with imm32. REX.W sign-extends imm32.
    const bool acc_wide=b[0]==0x48;
    if (!acc_wide || memory.fetch(state.rip,std::span(b).first(2))) {
        const auto op=acc_wide?b[1]:b[0];
        if (op==0x0d || op==0x25 || op==0x35) {
            const unsigned offset=acc_wide?2:1, length=offset+4;
            if (!memory.fetch(state.rip,std::span(b).first(length))) return StepResult::memory_fault;
            const auto imm=little(b.data()+offset,4);
            const unsigned ext=op==0x0d?1:op==0x25?4:6;
            if (acc_wide) group_logical64(state,state.rax,sign_extend(imm,32),ext);
            else group_logical32(state,state.rax,static_cast<std::uint32_t>(imm),ext);
            state.rip+=length; return StepResult::running;
        }
    } else return StepResult::memory_fault;
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
        if (b[1]==0x05) return StepResult::syscall_trap; // Runtime decides whether/how to handle it.
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

#include "runtime/linux_process.h"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
namespace nightfall::runtime {
namespace {
constexpr std::size_t stack_limit=65536, arg_limit=128, arg_bytes_limit=32768;
void put64(std::vector<std::uint8_t>& data,std::size_t pos,std::uint64_t v) {
    for(unsigned i=0;i<8;++i) data[pos+i]=static_cast<std::uint8_t>(v>>(8*i));
}
std::uint64_t err(unsigned code) { return std::uint64_t{0}-code; }
}
bool prepare_linux_stack(Machine& machine,std::uint64_t base,std::size_t size,std::span<const std::string> args) {
    if(!size || size>stack_limit || args.size()>arg_limit || base>machine.memory.size() || size>machine.memory.size()-base) return false;
    std::size_t bytes=0;
    for(const auto& arg:args) {
        if(arg.find('\0')!=std::string::npos || arg.size()>=arg_bytes_limit || bytes>arg_bytes_limit-arg.size()-1) return false;
        bytes+=arg.size()+1;
    }
    const auto words=1+args.size()+1+1+2;
    if(bytes>size || words>(size-bytes)/8) return false;
    try {
        std::vector<std::uint8_t> buffer(size,0);
        std::vector<std::uint64_t> pointers(args.size());
        std::size_t cursor=size;
        for(std::size_t i=args.size();i>0;--i) {
            const auto& arg=args[i-1]; cursor-=arg.size()+1;
            std::copy(arg.begin(),arg.end(),buffer.begin()+cursor); pointers[i-1]=base+cursor;
        }
        const auto unaligned=base+cursor-words*8;
        const auto rsp=unaligned & ~std::uint64_t{15};
        if(rsp<base) return false;
        const auto start=static_cast<std::size_t>(rsp-base);
        auto pos=start; put64(buffer,pos,args.size()); pos+=8;
        for(auto p:pointers) { put64(buffer,pos,p); pos+=8; }
        put64(buffer,pos,0); pos+=8; // argv terminator
        put64(buffer,pos,0); pos+=8; // empty environment
        put64(buffer,pos,0); pos+=8; put64(buffer,pos,0); // AT_NULL, value
        if(!machine.memory.write(rsp,std::span<const std::uint8_t>(buffer).subspan(start))) return false;
        *machine.cpu.state.reg(4)=rsp;
        return true;
    } catch(const std::bad_alloc&) { return false; } catch(const std::length_error&) { return false; }
}
LinuxProcess::LinuxProcess(Machine& machine,std::size_t max_output) : machine_(machine),max_output_(max_output) {}
bool LinuxProcess::set_stdin(std::span<const std::uint8_t> bytes) {
    if(started_ || exited_ || bytes.size()>65536) return false;
    try { std::vector<std::uint8_t> staged; if(!bytes.empty()) staged.assign(bytes.begin(),bytes.end()); stdin_=std::move(staged); stdin_cursor_=0; return true; }
    catch(const std::bad_alloc&) { return false; } catch(const std::length_error&) { return false; }
}
bool LinuxProcess::configure_brk(std::uint64_t base,std::uint64_t limit) {
    constexpr std::uint64_t page=4096, max_heap=1024*1024;
    if(started_ || exited_ || brk_configured_ || base>=limit || (base&(page-1)) || (limit&(page-1)) ||
       limit>machine_.memory.size() || limit-base>max_heap) return false;
    brk_base_=brk_current_=brk_mapped_end_=base; brk_limit_=limit; brk_configured_=true; return true;
}
void LinuxProcess::dispatch() {
    auto& state=machine_.cpu.state;
    const auto number=state.rax, arg0=*state.reg(7), arg1=*state.reg(6), arg2=*state.reg(2);
    // Userspace-observable syscall return registers. Kernel mode and FMASK are not modeled.
    *state.reg(1)=state.rip+2; *state.reg(11)=state.rflags; state.rip+=2;
    if(number==60 || number==231) { exited_=true; exit_code_=static_cast<std::uint8_t>(arg0); return; }
    if(number==12) { // brk: bounded synthetic heap, no host memory mapping
        if(!brk_configured_) { state.rax=err(38); return; }
        if(!arg0 || arg0<brk_base_ || arg0>brk_limit_) { state.rax=brk_current_; return; }
        constexpr std::uint64_t page=4096;
        const auto offset=arg0-brk_base_;
        const auto end=brk_base_+((offset+page-1)&~(page-1));
        if(end>brk_mapped_end_) {
            const auto status=machine_.memory.map_detailed(brk_mapped_end_,static_cast<std::size_t>(end-brk_mapped_end_),Permission::read|Permission::write);
            if(status!=MapStatus::ok) { state.rax=brk_current_; return; }
        } else if(end<brk_mapped_end_) {
            if(!machine_.memory.unmap(end,static_cast<std::size_t>(brk_mapped_end_-end))) { state.rax=brk_current_; return; }
        }
        brk_current_=arg0; brk_mapped_end_=end; state.rax=brk_current_; return;
    }
    if(number==19) { // readv: input and destination validation before mutation
        const auto fd=static_cast<std::uint32_t>(arg0), count=static_cast<std::uint32_t>(arg2);
        if(fd!=0) { state.rax=err(9); return; }
        if(count>1024) { state.rax=err(22); return; }
        if(!count) { state.rax=0; return; }
        try {
            std::vector<std::uint8_t> table(static_cast<std::size_t>(count)*16);
            if(!machine_.memory.read(arg1,table)) { state.rax=err(14); return; }
            auto read64=[](std::span<const std::uint8_t> b,std::size_t pos) {
                std::uint64_t v=0; for(unsigned i=0;i<8;++i) v|=std::uint64_t(b[pos+i])<<(8*i); return v;
            };
            std::uint64_t total=0;
            for(std::uint32_t i=0;i<count;++i) {
                const auto len=read64(table,static_cast<std::size_t>(i)*16+8);
                if(len>std::uint64_t{0x7fffffffffffffffULL}-total) { state.rax=err(22); return; }
                total+=len;
            }
            const auto available=stdin_.size()-stdin_cursor_;
            const auto amount=total<available?static_cast<std::size_t>(total):available;
            // Validate only the portions that this short read will actually touch.
            std::size_t left=amount;
            for(std::uint32_t i=0;i<count && left;++i) {
                const auto off=static_cast<std::size_t>(i)*16, len=read64(table,off+8);
                const auto n=len<left?static_cast<std::size_t>(len):left;
                if(n && !machine_.memory.can_access(read64(table,off),n,Permission::write)) { state.rax=err(14); return; }
                left-=n;
            }
            left=amount; std::size_t consumed=0;
            for(std::uint32_t i=0;i<count && left;++i) {
                const auto off=static_cast<std::size_t>(i)*16, len=read64(table,off+8);
                const auto n=len<left?static_cast<std::size_t>(len):left;
                if(n && !machine_.memory.write(read64(table,off),std::span<const std::uint8_t>(stdin_).subspan(stdin_cursor_+consumed,n))) { state.rax=err(14); return; }
                left-=n; consumed+=n;
            }
            stdin_cursor_+=amount; state.rax=amount; return;
        } catch(const std::bad_alloc&) { state.rax=err(12); return; }
          catch(const std::length_error&) { state.rax=err(12); return; }
    }
    if(number==20) { // writev: bounded, validated guest iovec array
        const auto fd=static_cast<std::uint32_t>(arg0), count=static_cast<std::uint32_t>(arg2);
        if(fd!=1 && fd!=2) { state.rax=err(9); return; }
        if(count>1024) { state.rax=err(22); return; } // EINVAL
        if(!count) { state.rax=0; return; }
        if(stdout_.size()>max_output_ || stderr_.size()>max_output_-stdout_.size()) { state.rax=err(27); return; }
        const auto remaining=max_output_-stdout_.size()-stderr_.size();
        try {
            std::vector<std::uint8_t> table(static_cast<std::size_t>(count)*16);
            if(!machine_.memory.read(arg1,table)) { state.rax=err(14); return; }
            auto read64=[](std::span<const std::uint8_t> b,std::size_t pos) {
                std::uint64_t v=0; for(unsigned i=0;i<8;++i) v|=std::uint64_t(b[pos+i])<<(8*i); return v;
            };
            std::uint64_t total=0;
            for(std::uint32_t i=0;i<count;++i) {
                const auto len=read64(table,static_cast<std::size_t>(i)*16+8);
                if(len>std::uint64_t{0x7fffffffffffffffULL}-total) { state.rax=err(22); return; }
                total+=len;
            }
            if(total>remaining || total>std::numeric_limits<std::size_t>::max()) { state.rax=err(27); return; }
            std::vector<std::uint8_t> bytes(static_cast<std::size_t>(total)); std::size_t pos=0;
            for(std::uint32_t i=0;i<count;++i) {
                const auto offset=static_cast<std::size_t>(i)*16;
                const auto address=read64(table,offset), len=static_cast<std::size_t>(read64(table,offset+8));
                if(len && !machine_.memory.read(address,std::span(bytes).subspan(pos,len))) { state.rax=err(14); return; }
                pos+=len;
            }
            auto& out=fd==1?stdout_:stderr_; out.insert(out.end(),bytes.begin(),bytes.end()); state.rax=total; return;
        } catch(const std::bad_alloc&) { state.rax=err(12); return; }
          catch(const std::length_error&) { state.rax=err(12); return; }
    }
    if(number==0) { // read: synthetic pre-supplied stdin only
        if(static_cast<std::uint32_t>(arg0)!=0) { state.rax=err(9); return; }
        const auto available=stdin_.size()-stdin_cursor_;
        const auto amount=arg2<available?static_cast<std::size_t>(arg2):available;
        if(!amount) { state.rax=0; return; }
        if(!machine_.memory.write(arg1,std::span<const std::uint8_t>(stdin_).subspan(stdin_cursor_,amount))) { state.rax=err(14); return; }
        stdin_cursor_+=amount; state.rax=amount; return;
    }
    if(number!=1) { state.rax=err(38); return; } // ENOSYS
    const auto fd=static_cast<std::uint32_t>(arg0); // Linux write receives an unsigned int fd.
    if(fd!=1 && fd!=2) { state.rax=err(9); return; } // EBADF
    if(arg2==0) { state.rax=0; return; }
    if(stdout_.size()>max_output_ || stderr_.size()>max_output_-stdout_.size()) { state.rax=err(27); return; }
    const auto used=stdout_.size()+stderr_.size();
    if(arg2>max_output_-used || arg2>std::numeric_limits<std::size_t>::max()) { state.rax=err(27); return; } // EFBIG, project limit
    try {
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(arg2));
        if(!machine_.memory.read(arg1,bytes)) { state.rax=err(14); return; } // EFAULT
        auto& out=fd==1?stdout_:stderr_;
        out.insert(out.end(),bytes.begin(),bytes.end()); state.rax=arg2;
    } catch(const std::bad_alloc&) { state.rax=err(12); } // ENOMEM
      catch(const std::length_error&) { state.rax=err(12); }
}
ProcessReport LinuxProcess::run(std::size_t budget) {
    ProcessReport r;
    if(exited_) { r.result=ProcessResult::already_exited; r.exit_code=exit_code_; r.stop_rip=machine_.cpu.state.rip; return r; }
    if(budget) started_=true;
    for(std::size_t i=0;i<budget;++i) {
        const auto before=machine_.cpu.state.rip; const auto step=machine_.cpu.step(machine_.memory); ++r.attempted;
        if(step==StepResult::running) continue;
        if(step==StepResult::syscall_trap) {
            dispatch();
            if(!exited_) continue;
            r.result=ProcessResult::exited; r.exit_code=exit_code_; r.stop_rip=before; return r;
        }
        r.stop_rip=before;
        r.result=step==StepResult::halted?ProcessResult::halted:step==StepResult::invalid_instruction?ProcessResult::invalid_instruction:ProcessResult::memory_fault;
        return r;
    }
    r.stop_rip=machine_.cpu.state.rip; return r;
}
}

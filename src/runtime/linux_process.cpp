#include "runtime/linux_process.h"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
namespace nightfall::runtime {
namespace {
constexpr std::size_t stack_limit=65536, arg_limit=128, arg_bytes_limit=32768;
template<class Buffer> void put64(Buffer& data,std::size_t pos,std::uint64_t v) {
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
LinuxProcess::LinuxProcess(Machine& machine,std::size_t max_output) : machine_(machine),max_output_(max_output) {
    descriptors_[0].kind=FdKind::input; descriptors_[1].kind=FdKind::output; descriptors_[2].kind=FdKind::error;
}
bool LinuxProcess::add_test_file(std::string path,std::span<const std::uint8_t> bytes) {
    constexpr std::size_t max_files=8,max_file=65536,max_total=262144;
    if(started_ || exited_ || path.empty() || path[0]!='/' || path.size()>255 || path.find('\0')!=std::string::npos ||
       files_.size()>=max_files || bytes.size()>max_file || bytes.size()>max_total-file_bytes_) return false;
    for(const auto& file:files_) if(file.path==path) return false;
    try {
        TestFile file; file.path=std::move(path); if(!bytes.empty()) file.bytes.assign(bytes.begin(),bytes.end());
        files_.push_back(std::move(file)); file_bytes_+=bytes.size(); return true;
    } catch(const std::bad_alloc&) { return false; } catch(const std::length_error&) { return false; }
}
void LinuxProcess::close_descriptor(std::size_t fd) noexcept {
    auto& desc=descriptors_[fd];
    if(desc.kind==FdKind::file) --open_files_[desc.description].references;
    desc={};
}
LinuxProcess::ReadSource LinuxProcess::read_source(std::uint32_t fd) noexcept {
    if(fd>=descriptors_.size()) return {};
    auto& desc=descriptors_[fd];
    if(desc.kind==FdKind::input) return {&stdin_,&stdin_cursor_};
    if(desc.kind==FdKind::file) { auto& open=open_files_[desc.description]; return {&files_[open.file_index].bytes,&open.cursor}; }
    return {};
}
bool LinuxProcess::set_stdin(std::span<const std::uint8_t> bytes) {
    if(started_ || exited_ || bytes.size()>65536) return false;
    try { std::vector<std::uint8_t> staged; if(!bytes.empty()) staged.assign(bytes.begin(),bytes.end()); stdin_=std::move(staged); stdin_cursor_=0; return true; }
    catch(const std::bad_alloc&) { return false; } catch(const std::length_error&) { return false; }
}
unsigned LinuxProcess::lookup_test_file(std::uint64_t address,std::size_t& index) {
    try {
        std::string path; bool terminated=false;
        for(std::size_t i=0;i<256;++i) {
            if(address>std::numeric_limits<std::uint64_t>::max()-i) return 14;
            std::array<std::uint8_t,1> ch{};
            if(!machine_.memory.read(address+i,ch)) return 14;
            if(!ch[0]) { terminated=true; break; }
            path.push_back(static_cast<char>(ch[0]));
        }
        if(!terminated) return 36;
        index=0; while(index<files_.size() && files_[index].path!=path) ++index;
        return index==files_.size()?2:0;
    } catch(const std::bad_alloc&) { return 12; } catch(const std::length_error&) { return 12; }
}
bool LinuxProcess::write_test_stat(std::size_t index,std::uint64_t address) {
    // Linux x86-64 struct stat, 144 bytes. Metadata is synthetic, not a host
    // file identity, credential, timestamp or implementation-defined struct.
    std::array<std::uint8_t,144> stat{};
    const auto size=files_[index].bytes.size();
    put64(stat,8,index+1); put64(stat,16,1); // synthetic inode, link count
    stat[24]=0x24; stat[25]=0x81; // S_IFREG | 0444, little endian
    put64(stat,48,size); put64(stat,56,4096); put64(stat,64,(size+511)/512);
    return machine_.memory.write(address,stat);
}
bool LinuxProcess::configure_brk(std::uint64_t base,std::uint64_t limit) {
    constexpr std::uint64_t page=4096, max_heap=1024*1024;
    if(started_ || exited_ || brk_configured_ || base>=limit || (base&(page-1)) || (limit&(page-1)) ||
       limit>machine_.memory.size() || limit-base>max_heap) return false;
    brk_base_=brk_current_=brk_mapped_end_=base; brk_limit_=limit; brk_configured_=true; return true;
}
bool LinuxProcess::configure_mmap(std::uint64_t base,std::uint64_t limit) {
    constexpr std::uint64_t page=4096, max_arena=4*1024*1024;
    if(started_ || exited_ || mmap_configured_ || base>=limit || (base&(page-1)) || (limit&(page-1)) ||
       limit>machine_.memory.size() || limit-base>max_arena) return false;
    mmap_base_=base; mmap_limit_=limit; mmap_configured_=true; return true;
}
void LinuxProcess::dispatch() {
    auto& state=machine_.cpu.state;
    const auto number=state.rax, arg0=*state.reg(7), arg1=*state.reg(6), arg2=*state.reg(2);
    // Userspace-observable syscall return registers. Kernel mode and FMASK are not modeled.
    *state.reg(1)=state.rip+2; *state.reg(11)=state.rflags; state.rip+=2;
    if(number==60 || number==231) { exited_=true; exit_code_=static_cast<std::uint8_t>(arg0); return; }
    if(number==2 || number==257) { // open/openat: immutable synthetic files only
        const auto path_address=number==2?arg0:arg1; const auto flags=static_cast<std::uint32_t>(number==2?arg1:arg2);
        if(number==257 && static_cast<std::uint32_t>(arg0)!=0xffffff9cU) { state.rax=err(9); return; } // AT_FDCWD
        if(flags&3) { state.rax=err(30); return; } // read-only fixture table
        if(flags&~0x80000U) { state.rax=err(22); return; } // optional O_CLOEXEC
        std::size_t file=0; const auto error=lookup_test_file(path_address,file);
        if(error) { state.rax=err(error); return; }
        std::size_t fd=0; while(fd<descriptors_.size() && descriptors_[fd].kind!=FdKind::unused) ++fd;
        if(fd==descriptors_.size()) { state.rax=err(24); return; }
        std::size_t slot=0; while(slot<open_files_.size() && open_files_[slot].references) ++slot;
        if(slot==open_files_.size()) { state.rax=err(24); return; }
        open_files_[slot]={file,1,0}; descriptors_[fd]={FdKind::file,slot,bool(flags&0x80000U)}; state.rax=fd; return;
    }
    if(number==3) { // close any modeled descriptor; no host descriptor exists.
        const auto fd=static_cast<std::uint32_t>(arg0);
        if(fd>=descriptors_.size() || descriptors_[fd].kind==FdKind::unused) { state.rax=err(9); return; }
        close_descriptor(fd); state.rax=0; return;
    }
    if(number==32 || number==33 || number==292) { // dup/dup2/dup3, bounded virtual descriptors
        const auto oldfd=static_cast<std::uint32_t>(arg0);
        if(oldfd>=descriptors_.size() || descriptors_[oldfd].kind==FdKind::unused) { state.rax=err(9); return; }
        std::size_t target=0;
        if(number==32) {
            while(target<descriptors_.size() && descriptors_[target].kind!=FdKind::unused) ++target;
            if(target==descriptors_.size()) { state.rax=err(24); return; }
        } else {
            target=static_cast<std::uint32_t>(arg1);
            if(target>=descriptors_.size()) { state.rax=err(9); return; }
            if(number==292 && (static_cast<std::uint32_t>(arg2) & ~0x80000U)) { state.rax=err(22); return; }
            if(oldfd==target) { state.rax=number==292?err(22):target; return; }
            close_descriptor(target);
        }
        descriptors_[target]=descriptors_[oldfd];
        descriptors_[target].cloexec=number==292 && (static_cast<std::uint32_t>(arg2)&0x80000U);
        if(descriptors_[target].kind==FdKind::file) ++open_files_[descriptors_[target].description].references;
        state.rax=target; return;
    }
    if(number==72) { // narrow fcntl: descriptor flags and duplication only
        const auto fd=static_cast<std::uint32_t>(arg0), command=static_cast<std::uint32_t>(arg1);
        if(fd>=descriptors_.size() || descriptors_[fd].kind==FdKind::unused) { state.rax=err(9); return; }
        if(command==1) { state.rax=descriptors_[fd].cloexec?1:0; return; } // F_GETFD
        if(command==2) { descriptors_[fd].cloexec=bool(arg2&1); state.rax=0; return; } // F_SETFD
        if(command==0 || command==1030) { // F_DUPFD / F_DUPFD_CLOEXEC
            const auto lower=static_cast<std::uint32_t>(arg2);
            if(lower>=descriptors_.size()) { state.rax=err(22); return; }
            std::size_t target=lower;
            while(target<descriptors_.size() && descriptors_[target].kind!=FdKind::unused) ++target;
            if(target==descriptors_.size()) { state.rax=err(24); return; }
            descriptors_[target]=descriptors_[fd]; descriptors_[target].cloexec=command==1030;
            if(descriptors_[target].kind==FdKind::file) ++open_files_[descriptors_[target].description].references;
            state.rax=target; return;
        }
        state.rax=err(22); return; // Other fcntl commands are not modeled.
    }
    if(number==4 || number==5 || number==6 || number==262) { // synthetic x86-64 stat
        std::size_t index=0; std::uint64_t destination=arg1;
        if(number==5) {
            const auto fd=static_cast<std::uint32_t>(arg0);
            if(fd>=descriptors_.size() || descriptors_[fd].kind==FdKind::unused) { state.rax=err(9); return; }
            if(descriptors_[fd].kind!=FdKind::file) { state.rax=err(38); return; }
            index=open_files_[descriptors_[fd].description].file_index;
        } else {
            auto path=arg0;
            if(number==262) {
                if(static_cast<std::uint32_t>(arg0)!=0xffffff9cU) { state.rax=err(9); return; }
                if(*state.reg(10)!=0) { state.rax=err(22); return; }
                path=arg1; destination=arg2;
            }
            const auto error=lookup_test_file(path,index); if(error) { state.rax=err(error); return; }
        }
        if(!write_test_stat(index,destination)) { state.rax=err(14); return; }
        state.rax=0; return;
    }
    if(number==8) { // lseek: synthetic regular files only, signed offset
        const auto fd=static_cast<std::uint32_t>(arg0), whence=static_cast<std::uint32_t>(arg2);
        if(fd>=descriptors_.size() || descriptors_[fd].kind==FdKind::unused) { state.rax=err(9); return; }
        auto& desc=descriptors_[fd]; if(desc.kind!=FdKind::file) { state.rax=err(29); return; }
        if(whence>2) { state.rax=err(22); return; }
        auto& open=open_files_[desc.description];
        const std::uint64_t origin=whence==0?0:whence==1?open.cursor:files_[open.file_index].bytes.size();
        constexpr auto max_offset=std::uint64_t{0x7fffffffffffffffULL};
        std::uint64_t target=0;
        if(arg1>>63) { const auto magnitude=~arg1+1; if(magnitude>origin) { state.rax=err(22); return; } target=origin-magnitude; }
        else { if(arg1>max_offset-origin) { state.rax=err(22); return; } target=origin+arg1; }
        open.cursor=target; state.rax=target; return;
    }
    if(number==9 || number==10 || number==11) { // bounded anonymous mmap/mprotect/munmap subset
        if(!mmap_configured_) { state.rax=err(38); return; }
        constexpr std::uint64_t page=4096, max_map=1024*1024;
        if(!arg1 || arg1>max_map) { state.rax=err(22); return; }
        const auto rounded=(arg1+page-1)&~(page-1);
        const auto pages=static_cast<std::size_t>(rounded/page);
        const auto arena_pages=static_cast<std::size_t>((mmap_limit_-mmap_base_)/page);
        if(number==9) {
            // Only null hint, private anonymous mappings with fd=-1, offset=0.
            const auto flags=*state.reg(10), fd=*state.reg(8), offset=*state.reg(9);
            if(arg0 || arg2>7 || flags!=0x22 || static_cast<std::uint32_t>(fd)!=0xffffffffu || offset) { state.rax=err(22); return; }
            if(pages>arena_pages) { state.rax=err(12); return; }
            for(std::size_t first=0;first+pages<=arena_pages;++first) {
                bool occupied=false; for(std::size_t j=0;j<pages;++j) if(mmap_pages_[first+j]) { first+=j; occupied=true; break; }
                if(occupied) continue;
                const auto address=mmap_base_+first*page;
                const auto status=machine_.memory.map_detailed(address,static_cast<std::size_t>(rounded),static_cast<Permission>(arg2));
                if(status==MapStatus::overlap) continue;
                if(status!=MapStatus::ok) { state.rax=err(12); return; }
                for(std::size_t j=0;j<pages;++j) mmap_pages_[first+j]=true;
                state.rax=address; return;
            }
            state.rax=err(12); return;
        }
        if((arg0&(page-1)) || arg0<mmap_base_ || arg0>=mmap_limit_ || rounded>mmap_limit_-arg0) { state.rax=err(22); return; }
        const auto first=static_cast<std::size_t>((arg0-mmap_base_)/page);
        if(number==10) {
            if(arg2>7) { state.rax=err(22); return; }
            for(std::size_t j=0;j<pages;++j) if(!mmap_pages_[first+j]) { state.rax=err(12); return; }
            if(!machine_.memory.protect(arg0,static_cast<std::size_t>(rounded),static_cast<Permission>(arg2))) { state.rax=err(12); return; }
            state.rax=0; return;
        }
        // Ignore holes inside the arena, but never unmap pages owned by the
        // loader, heap, or external host-side setup. Validate before mutation.
        for(std::size_t j=0;j<pages;++j) if(mmap_pages_[first+j] && !machine_.memory.can_access(arg0+j*page,page,Permission::none)) { state.rax=err(12); return; }
        for(std::size_t j=0;j<pages;++j) if(mmap_pages_[first+j]) {
            if(!machine_.memory.unmap(arg0+j*page,page)) { state.rax=err(12); return; }
            mmap_pages_[first+j]=false;
        }
        state.rax=0; return;
    }
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
        const auto source=read_source(fd);
        if(!source.bytes) { state.rax=err(9); return; }
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
            const auto cursor=*source.cursor;
            const auto available=cursor>=source.bytes->size()?std::size_t{0}:source.bytes->size()-static_cast<std::size_t>(cursor);
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
                if(n && !machine_.memory.write(read64(table,off),std::span<const std::uint8_t>(*source.bytes).subspan(static_cast<std::size_t>(cursor)+consumed,n))) { state.rax=err(14); return; }
                left-=n; consumed+=n;
            }
            *source.cursor+=amount; state.rax=amount; return;
        } catch(const std::bad_alloc&) { state.rax=err(12); return; }
          catch(const std::length_error&) { state.rax=err(12); return; }
    }
    if(number==20) { // writev: bounded, validated guest iovec array
        const auto fd=static_cast<std::uint32_t>(arg0), count=static_cast<std::uint32_t>(arg2);
        if(fd>=descriptors_.size() || (descriptors_[fd].kind!=FdKind::output && descriptors_[fd].kind!=FdKind::error)) { state.rax=err(9); return; }
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
            auto& out=descriptors_[fd].kind==FdKind::output?stdout_:stderr_; out.insert(out.end(),bytes.begin(),bytes.end()); state.rax=total; return;
        } catch(const std::bad_alloc&) { state.rax=err(12); return; }
          catch(const std::length_error&) { state.rax=err(12); return; }
    }
    if(number==0 || number==17) { // read/pread64: synthetic inputs only
        const auto fd=static_cast<std::uint32_t>(arg0); const auto source=read_source(fd);
        if(!source.bytes) { state.rax=err(9); return; }
        if(number==17 && descriptors_[fd].kind!=FdKind::file) { state.rax=err(29); return; }
        const auto cursor=number==17?*state.reg(10):*source.cursor;
        if(number==17 && (cursor>>63)) { state.rax=err(22); return; }
        const auto available=cursor>=source.bytes->size()?std::size_t{0}:source.bytes->size()-static_cast<std::size_t>(cursor);
        const auto amount=arg2<available?static_cast<std::size_t>(arg2):available;
        if(!amount) { state.rax=0; return; }
        if(!machine_.memory.write(arg1,std::span<const std::uint8_t>(*source.bytes).subspan(static_cast<std::size_t>(cursor),amount))) { state.rax=err(14); return; }
        if(number==0) *source.cursor+=amount;
        state.rax=amount; return;
    }
    if(number!=1) { state.rax=err(38); return; } // ENOSYS
    const auto fd=static_cast<std::uint32_t>(arg0); // Linux write receives an unsigned int fd.
    if(fd>=descriptors_.size() || (descriptors_[fd].kind!=FdKind::output && descriptors_[fd].kind!=FdKind::error)) { state.rax=err(9); return; } // EBADF
    if(arg2==0) { state.rax=0; return; }
    if(stdout_.size()>max_output_ || stderr_.size()>max_output_-stdout_.size()) { state.rax=err(27); return; }
    const auto used=stdout_.size()+stderr_.size();
    if(arg2>max_output_-used || arg2>std::numeric_limits<std::size_t>::max()) { state.rax=err(27); return; } // EFBIG, project limit
    try {
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(arg2));
        if(!machine_.memory.read(arg1,bytes)) { state.rax=err(14); return; } // EFAULT
        auto& out=descriptors_[fd].kind==FdKind::output?stdout_:stderr_;
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
        r.result=step==StepResult::halted?ProcessResult::halted:step==StepResult::invalid_instruction?ProcessResult::invalid_instruction:step==StepResult::divide_error?ProcessResult::divide_error:step==StepResult::alignment_fault?ProcessResult::alignment_fault:ProcessResult::memory_fault;
        return r;
    }
    r.stop_rip=machine_.cpu.state.rip; return r;
}
}

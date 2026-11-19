#include "loader/elf_inspect.h"
#include "storage/storage.h"
#include <cstdint>
#include <span>
#include <system_error>
namespace nightfall::loader {
namespace {
std::uint64_t le(std::span<const std::uint8_t> data,std::size_t pos,unsigned count) {
    std::uint64_t v=0; for(unsigned i=0;i<count;++i) v|=std::uint64_t(data[pos+i])<<(8*i); return v;
}
bool fits(std::uint64_t offset,std::uint64_t n,std::uint64_t size) { return offset<=size && n<=size-offset; }
}
const char* inspect_status_name(InspectStatus status) noexcept {
    switch(status) {
    case InspectStatus::ok: return "ok";
    case InspectStatus::read_error: return "read_error";
    case InspectStatus::not_elf: return "not_elf";
    case InspectStatus::unsupported_format: return "unsupported_format";
    case InspectStatus::malformed: return "malformed";
    }
    return "unknown";
}
ElfInspection inspect_elf64(const std::filesystem::path& path) {
    ElfInspection out; std::error_code ec; out.file_size=std::filesystem::file_size(path,ec);
    if(ec) return out;
    if(out.file_size<4) { out.status=InspectStatus::not_elf; return out; }
    // Read only the small ELF header and bounded program-header table, never segments.
    const auto n=static_cast<std::size_t>(out.file_size<64?out.file_size:64);
    auto head=storage::read_range(path,0,n,64);
    if(head.status!=storage::ReadStatus::ok) return out;
    const auto& b=head.bytes;
    if(b[0]!=0x7f || b[1]!='E' || b[2]!='L' || b[3]!='F') { out.status=InspectStatus::not_elf; return out; }
    if(n<16) { out.status=InspectStatus::malformed; return out; }
    if(b[4]!=2 || b[5]!=1 || b[6]!=1) { out.status=InspectStatus::unsupported_format; return out; }
    if(n<64 || le(b,20,4)!=1 || le(b,52,2)!=64) { out.status=InspectStatus::malformed; return out; }
    out.osabi=b[7]; out.abi_version=b[8]; out.type=le(b,16,2); out.machine=le(b,18,2); out.entry=le(b,24,8);
    const auto phoff=le(b,32,8), entsize=le(b,54,2), count=le(b,56,2);
    if(count==0xffff || count>128) { out.status=InspectStatus::unsupported_format; return out; }
    if(count && (entsize!=56 || !fits(phoff,count*56,out.file_size))) { out.status=InspectStatus::malformed; return out; }
    if(count) {
        const auto bytes=static_cast<std::size_t>(count*56);
        auto table=storage::read_range(path,phoff,bytes,128*56);
        if(table.status!=storage::ReadStatus::ok) return out;
        out.segments.reserve(static_cast<std::size_t>(count));
        for(std::size_t i=0;i<count;++i) {
            const auto p=i*56;
            InspectSegment segment{static_cast<std::uint32_t>(le(table.bytes,p,4)),static_cast<std::uint32_t>(le(table.bytes,p+4,4)),
                le(table.bytes,p+8,8),le(table.bytes,p+16,8),le(table.bytes,p+32,8),le(table.bytes,p+40,8),le(table.bytes,p+48,8)};
            if(segment.type==1 && (segment.file_size>segment.memory_size || !fits(segment.offset,segment.file_size,out.file_size) ||
               (segment.alignment>1 && ((segment.alignment&(segment.alignment-1)) || ((segment.address^segment.offset)&(segment.alignment-1)))))) {
                out.segments.clear(); out.status=InspectStatus::malformed; return out;
            }
            out.segments.push_back(segment);
        }
    }
    out.status=InspectStatus::ok; return out;
}
}

#include "loader/static_pie.h"
#include <array>
#include <limits>
#include <vector>
#include <utility>
namespace nightfall::loader {
namespace {
std::uint64_t le(std::span<const std::uint8_t> b,std::size_t pos,unsigned n) {
 std::uint64_t value=0;for(unsigned i=0;i<n;++i)value|=std::uint64_t(b[pos+i])<<(8*i);return value;
}
bool fits(std::uint64_t off,std::uint64_t len,std::uint64_t size){return off<=size && len<=size-off;}
bool add(std::uint64_t a,std::uint64_t b,std::uint64_t& out){if(b>UINT64_MAX-a)return false;out=a+b;return true;}
struct Segment {std::uint64_t offset,address,filesz,memsz,align;std::uint32_t flags;};
// Translate a file-backed virtual range through a single PT_LOAD.
bool file_offset(const std::vector<Segment>& segments,std::uint64_t address,std::uint64_t count,std::uint64_t& out){
 for(const auto& s:segments)if(address>=s.address && fits(address-s.address,count,s.filesz)){out=s.offset+(address-s.address);return true;}
 return false;
}
}
ElfStatus load_static_pie(std::span<const std::uint8_t> file,Machine& machine,std::uint64_t bias,std::uint64_t stack_base,std::size_t stack_size,std::span<const ImportBinding> bindings,std::string* missing_symbol){
 if(missing_symbol)missing_symbol->clear();
 if(bindings.size()>1024)return ElfStatus::resource_limit;
 for(std::size_t i=0;i<bindings.size();++i){
  if(bindings[i].name.empty()||bindings[i].name.size()>255||bindings[i].name.find('\0')!=std::string_view::npos||bindings[i].guest_address>=machine.memory.size())return ElfStatus::malformed;
  for(std::size_t j=0;j<i;++j)if(bindings[i].name==bindings[j].name)return ElfStatus::malformed;
 }
 if(file.size()<64||file[0]!=0x7f||file[1]!='E'||file[2]!='L'||file[3]!='F')return ElfStatus::malformed;
 if(file[4]!=2||file[5]!=1||file[6]!=1||le(file,16,2)!=3||le(file,18,2)!=62||le(file,20,4)!=1)return ElfStatus::unsupported;
 const auto entry=le(file,24,8),phoff=le(file,32,8),count=le(file,56,2);
 if(le(file,52,2)!=64||le(file,54,2)!=56||!count||count>128||!fits(phoff,count*56,file.size()))return ElfStatus::malformed;
 if(!stack_size||!fits(stack_base,stack_size,machine.memory.size()))return ElfStatus::range_error;
 std::uint64_t biased_entry=0;if(!add(bias,entry,biased_entry))return ElfStatus::range_error;
 std::vector<Segment> segments; bool found_dynamic=false;std::uint64_t dynaddr=0,dynoffset=0,dynsize=0;
 for(std::uint64_t i=0;i<count;++i){const auto at=static_cast<std::size_t>(phoff+i*56);
  const auto type=le(file,at,4),flags=le(file,at+4,4),offset=le(file,at+8,8),addr=le(file,at+16,8),filesz=le(file,at+32,8),memsz=le(file,at+40,8),align=le(file,at+48,8);
  if(type==3||type==7||type==0x6474e552)return ElfStatus::unsupported; // interpreter, TLS, RELRO
  if(type==0x6474e551 && (flags&1))return ElfStatus::unsupported; // executable stack not modeled
  if(type==2){if(found_dynamic||!filesz||filesz>memsz||filesz%16||filesz>16384||!fits(offset,filesz,file.size()))return ElfStatus::malformed;found_dynamic=true;dynaddr=addr;dynoffset=offset;dynsize=filesz;continue;}
  if(type!=1) {
   // Only these non-executing metadata headers are intentionally ignored.
   if(type==0||type==4||type==6||type==0x6474e550||type==0x6474e551||type==0x6474e553)continue;
   return ElfStatus::unsupported;
  }
  if(flags&~7ULL)return ElfStatus::unsupported;
  if(filesz>memsz||!fits(offset,filesz,file.size()))return ElfStatus::malformed;
  if(align>1&&((align&(align-1))||((addr^offset)&(align-1))||(bias&(align-1))))return ElfStatus::malformed;
  if(!memsz)continue;
  std::uint64_t dest=0;if(!add(bias,addr,dest)||!fits(dest,memsz,machine.memory.size()))return ElfStatus::range_error;
  segments.push_back({offset,addr,filesz,memsz,align,static_cast<std::uint32_t>(flags)});
 }
 if(segments.empty()||!found_dynamic)return ElfStatus::malformed;
 std::uint64_t translated=0;if(!file_offset(segments,dynaddr,dynsize,translated)||translated!=dynoffset)return ElfStatus::malformed;
 bool end=false,has_rela=false,has_size=false,has_ent=false,has_count=false,has_symtab=false,has_syment=false,has_strtab=false,has_strsz=false;
 std::uint64_t rela=0,relasz=0,relaent=0,relacount=0,symtab=0,syment=0,strtab=0,strsz=0;
 for(std::uint64_t off=0;off<dynsize;off+=16){auto pos=static_cast<std::size_t>(dynoffset+off);const auto tag=le(file,pos,8),val=le(file,pos+8,8);
  if(tag==0){end=true;break;}
  if(tag==7){if(has_rela)return ElfStatus::malformed;has_rela=true;rela=val;}
  else if(tag==8){if(has_size)return ElfStatus::malformed;has_size=true;relasz=val;}
  else if(tag==9){if(has_ent)return ElfStatus::malformed;has_ent=true;relaent=val;}
  else if(tag==0x6ffffff9){if(has_count)return ElfStatus::malformed;has_count=true;relacount=val;}
  else if(tag==5){if(has_strtab)return ElfStatus::malformed;has_strtab=true;strtab=val;}
  else if(tag==10){if(has_strsz)return ElfStatus::malformed;has_strsz=true;strsz=val;}
  else if(tag==6){if(has_symtab)return ElfStatus::malformed;has_symtab=true;symtab=val;}
  else if(tag==11){if(has_syment)return ElfStatus::malformed;has_syment=true;syment=val;}
  else if(tag==0x6ffffffb){if(val!=0x08000000)return ElfStatus::unsupported;} // DF_1_PIE only
  else if(tag==4||tag==21||tag==0x6ffffef5) { /* inert symbol/string/hash/debug metadata */ }
  else return ElfStatus::unsupported; // no libraries, PLT, constructors, textrels, REL/RELR, etc.
 }
 if(!end)return ElfStatus::malformed;
 if(has_rela!=has_size||has_rela!=has_ent||has_symtab!=has_syment||has_strtab!=has_strsz||(has_syment&&syment!=24))return ElfStatus::malformed;
 if(has_strsz&&strsz>65536)return ElfStatus::resource_limit;
 if(!has_rela && has_count && relacount)return ElfStatus::malformed;
 std::uint64_t relocoff=0;
 if(has_rela){if(relaent!=24||relasz%24||relasz>4096*24|| (has_count&&relacount>relasz/24))return ElfStatus::malformed;
  if(relasz&&!file_offset(segments,rela,relasz,relocoff))return ElfStatus::malformed;}
 Memory staged=machine.memory.is_sparse()?Memory(machine.memory.size(),SparsePages{machine.memory.max_pages()}):Memory(machine.memory.size(),false);
 for(const auto& s:segments){const auto dest=bias+s.address;const auto mapped=staged.map_detailed(dest,static_cast<std::size_t>(s.memsz),Permission::read|Permission::write);
  if(mapped==MapStatus::resource_limit)return ElfStatus::resource_limit;
  if(mapped!=MapStatus::ok)return ElfStatus::overlap;
  if(s.filesz&&!staged.write(dest,file.subspan(static_cast<std::size_t>(s.offset),static_cast<std::size_t>(s.filesz))))return ElfStatus::malformed;
 }
 for(std::uint64_t i=0;i<relasz/24;++i){auto p=static_cast<std::size_t>(relocoff+i*24);const auto target=le(file,p,8),info=le(file,p+8,8),addend=le(file,p+16,8);
  const auto type=static_cast<std::uint32_t>(info); const auto sym=info>>32;
  std::uint64_t value=0;
  if(type==8 && sym==0) value=bias+addend; // R_X86_64_RELATIVE
  else if((type==1||type==6) && sym>0 && sym<4096 && has_symtab) {
   // Defined symbols are self-resolved. Undefined imports require explicit
   // bindings to guest virtual addresses; no host symbol lookup is performed.
   std::uint64_t symbol=0,symoff=0;
   if(!add(symtab,sym*24,symbol)||!file_offset(segments,symbol,24,symoff))return ElfStatus::malformed;
   const auto pos=static_cast<std::size_t>(symoff); const auto info_byte=file[pos+4],other=file[pos+5];
   const auto section=le(file,pos+6,2),symval=le(file,pos+8,8);
   const auto binding=info_byte>>4,kind=info_byte&15;
   if((binding!=0 && binding!=1 && binding!=2)||kind>2||other>3||section>=0xff00)return ElfStatus::unsupported;
   if(!section) {
    if(binding==0 || other!=0 || !has_strtab || !strsz)return ElfStatus::unsupported;
    const auto nameoff=le(file,pos,4);std::uint64_t stroff=0;
    if(nameoff>=strsz||!file_offset(segments,strtab,strsz,stroff))return ElfStatus::malformed;
    const auto start=static_cast<std::size_t>(stroff+nameoff), limit=static_cast<std::size_t>(stroff+strsz);
    auto end_name=start;while(end_name<limit && end_name-start<=255 && file[end_name])++end_name;
    if(end_name==start||end_name==limit||end_name-start>255)return ElfStatus::malformed;
    const std::string_view name(reinterpret_cast<const char*>(file.data()+start),end_name-start);
    bool found=false;for(const auto& item:bindings)if(item.name==name){value=item.guest_address;found=true;break;}
    if(!found){if(missing_symbol)missing_symbol->assign(name);return ElfStatus::unresolved_symbol;}
   } else {
    bool inside=false;for(const auto& seg:segments)if(symval>=seg.address&&fits(symval-seg.address,1,seg.memsz)){inside=true;break;}
    if(!inside||!add(bias,symval,value))return ElfStatus::malformed;
   }
   if(type==1)value+=addend; // R_X86_64_64: S + A. GLOB_DAT: S.
  } else return ElfStatus::unsupported;
  bool writable=false;for(const auto& s:segments)if((s.flags&2)&&target>=s.address&&fits(target-s.address,8,s.memsz)){writable=true;break;}
  std::uint64_t address=0;if(!writable||!add(bias,target,address))return ElfStatus::malformed;
  std::array<std::uint8_t,8> bytes{};for(unsigned j=0;j<8;++j)bytes[j]=static_cast<std::uint8_t>(value>>(8*j));
  if(!staged.write(address,bytes))return ElfStatus::malformed;
 }
 for(const auto& s:segments){Permission perm=Permission::none;if(s.flags&4)perm=perm|Permission::read;if(s.flags&2)perm=perm|Permission::write;if(s.flags&1)perm=perm|Permission::execute;if(!staged.protect(bias+s.address,static_cast<std::size_t>(s.memsz),perm))return ElfStatus::malformed;}
 const auto stack=staged.map_detailed(stack_base,stack_size,Permission::read|Permission::write);
 if(stack==MapStatus::resource_limit)return ElfStatus::resource_limit;
 if(stack!=MapStatus::ok)return ElfStatus::overlap;
 std::array<std::uint8_t,1> probe{};if(!staged.fetch(biased_entry,probe))return ElfStatus::no_executable_entry;
 machine.memory=std::move(staged);machine.cpu.state=CpuState{};machine.cpu.state.rip=biased_entry;*machine.cpu.state.reg(4)=stack_base+stack_size;return ElfStatus::ok;
}
}

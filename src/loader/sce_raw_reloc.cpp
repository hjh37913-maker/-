#include "loader/sce_raw_reloc.h"
#include "loader/sce_id.h"
#include <array>
#include <utility>
#include <vector>
namespace nightfall::loader {
namespace {
std::uint64_t le(std::span<const std::uint8_t>b,std::size_t p,unsigned n){std::uint64_t x=0;for(unsigned i=0;i<n;++i)x|=std::uint64_t(b[p+i])<<(8*i);return x;}
bool fits(std::uint64_t p,std::uint64_t n,std::uint64_t s){return p<=s&&n<=s-p;}
bool add(std::uint64_t x,std::uint64_t y,std::uint64_t& z){if(y>UINT64_MAX-x)return false;z=x+y;return true;}
struct Segment{std::uint64_t offset,address,filesz,memsz;std::uint32_t flags;};
struct Field{std::uint64_t value=0;bool seen=false;};
}
ElfStatus load_sce_raw_relocatable(std::span<const std::uint8_t> file,Machine& machine,std::uint64_t bias,std::uint64_t stack_base,std::size_t stack_size,std::span<const ImportBinding> bindings,std::string* missing,std::span<const SceDependencyBinding> dependencies){
 if(missing)missing->clear();
 if(bindings.size()>1024||dependencies.size()>1024)return ElfStatus::resource_limit;
 for(std::size_t i=0;i<dependencies.size();++i){const auto& dep=dependencies[i];
  if((dep.kind!=SceDependencyKind::module&&dep.kind!=SceDependencyKind::library)||dep.name.empty()||dep.name.size()>255||dep.name.find('\0')!=std::string_view::npos)return ElfStatus::malformed;
  for(std::size_t j=0;j<i;++j)if(dep.kind==dependencies[j].kind&&dep.id==dependencies[j].id)return ElfStatus::malformed;
 }
 for(std::size_t i=0;i<bindings.size();++i){const auto& item=bindings[i];
  if(item.name.empty()||item.name.size()>255||item.name.find('\0')!=std::string_view::npos||item.guest_address>=machine.memory.size())return ElfStatus::malformed;
  for(std::size_t j=0;j<i;++j)if(item.name==bindings[j].name)return ElfStatus::malformed;
 }
 if(file.size()<64||file[0]!=0x7f||file[1]!='E'||file[2]!='L'||file[3]!='F')return ElfStatus::malformed;
 const auto type=le(file,16,2);if(file[4]!=2||file[5]!=1||file[6]!=1||file[7]!=9||le(file,18,2)!=62||le(file,20,4)!=1||(type!=0xfe10&&type!=0xfe18))return ElfStatus::unsupported;
 const auto entry=le(file,24,8),phoff=le(file,32,8),num=le(file,56,2);
 if(le(file,52,2)!=64||le(file,54,2)!=56||!num||num>128||!fits(phoff,num*56,file.size()))return ElfStatus::malformed;
 if(!stack_size||!fits(stack_base,stack_size,machine.memory.size()))return ElfStatus::range_error;
 std::uint64_t ip=0;if(!add(bias,entry,ip))return ElfStatus::range_error;
 std::vector<Segment> segments;bool hd=false,hdata=false;std::uint64_t d=0,ds=0,data=0,datasize=0;
 for(std::uint64_t i=0;i<num;++i){const auto p=static_cast<std::size_t>(phoff+i*56);const auto pt=le(file,p,4),flags=le(file,p+4,4),off=le(file,p+8,8),addr=le(file,p+16,8),fs=le(file,p+32,8),ms=le(file,p+40,8),align=le(file,p+48,8);
  if(pt==2||pt==0x61000000){if(!fits(off,fs,file.size())||!fs)return ElfStatus::malformed;
   if(pt==2){if(hd||fs>ms||fs%16||fs>16384)return ElfStatus::malformed;hd=true;d=off;ds=fs;}
   else {if(hdata)return ElfStatus::malformed;hdata=true;data=off;datasize=fs;}
   continue;}
  if(pt!=1){if(pt==0||pt==4||pt==6||pt==0x6474e550)continue;return ElfStatus::unsupported;}
  if(flags&~7ULL)return ElfStatus::unsupported;
  if(fs>ms||!fits(off,fs,file.size()))return ElfStatus::malformed;
  if(align>1&&((align&(align-1))||((off^addr)&(align-1))||(bias&(align-1))))return ElfStatus::malformed;
  if(!ms)continue;
  std::uint64_t dest=0;if(!add(bias,addr,dest)||!fits(dest,ms,machine.memory.size()))return ElfStatus::range_error;
  segments.push_back({off,addr,fs,ms,static_cast<std::uint32_t>(flags)});
 }
 if(!hd||!hdata||segments.empty())return ElfStatus::malformed;
 Field str,strsz,sym,symsz,syment,rela,relasz,relaent,jmp,jmpsz,jmptype;bool end=false;
 std::vector<std::pair<SceDependencyKind,std::uint64_t>> declared;
 for(std::uint64_t off=0;off<ds;off+=16){const auto p=static_cast<std::size_t>(d+off),tag=le(file,p,8),val=le(file,p+8,8);if(!tag){end=true;break;}Field* f=nullptr;
  if(tag==0x6100000f||tag==0x61000045||tag==0x61000015||tag==0x61000049){
   declared.emplace_back((tag==0x6100000f||tag==0x61000045)?SceDependencyKind::module:SceDependencyKind::library,val);continue;
  }
  switch(tag){case 0x61000035:f=&str;break;case 0x61000037:f=&strsz;break;case 0x61000039:f=&sym;break;case 0x6100003f:f=&symsz;break;case 0x6100003b:f=&syment;break;
   case 0x6100002f:f=&rela;break;case 0x61000031:f=&relasz;break;case 0x61000033:f=&relaent;break;case 0x61000029:f=&jmp;break;case 0x6100002d:f=&jmpsz;break;case 0x6100002b:f=&jmptype;break;
   default:return ElfStatus::unsupported;}
  if(f->seen)return ElfStatus::malformed;
  f->seen=true;f->value=val;
 }
 if(!end||!str.seen||!strsz.seen||!sym.seen||!symsz.seen||!syment.seen||syment.value!=24||symsz.value%24||!strsz.value)return ElfStatus::malformed;
 if(strsz.value>65536||symsz.value>4096*24)return ElfStatus::resource_limit;
 if(!fits(str.value,strsz.value,datasize)||!fits(sym.value,symsz.value,datasize))return ElfStatus::malformed;
 // Declarations are author-supplied metadata, not proof of a module being
 // implemented. Require explicit matching identities; never load host paths.
 for(std::size_t i=0;i<declared.size();++i){const auto [kind,raw]=declared[i];const auto id=static_cast<std::uint16_t>(raw>>48);
  for(std::size_t j=0;j<i;++j)if(declared[j].first==kind&&static_cast<std::uint16_t>(declared[j].second>>48)==id)return ElfStatus::malformed;
  const auto off=static_cast<std::uint32_t>(raw);if(off>=strsz.value)return ElfStatus::malformed;
  const auto begin=static_cast<std::size_t>(data+str.value+off),limit=static_cast<std::size_t>(data+str.value+strsz.value);auto e=begin;
  while(e<limit&&e-begin<=255&&file[e])++e;
  if(e==begin||e==limit||e-begin>255)return ElfStatus::malformed;
  const std::string_view name(reinterpret_cast<const char*>(file.data()+begin),e-begin);
  const auto major=static_cast<std::uint16_t>(kind==SceDependencyKind::module?(raw>>40)&0xff:(raw>>32)&0xffff);
  const auto minor=static_cast<std::uint16_t>(kind==SceDependencyKind::module?(raw>>32)&0xff:0);
  bool found=false;for(const auto& dep:dependencies)if(dep.kind==kind&&dep.id==id&&dep.version_major==major&&dep.version_minor==minor&&dep.name==name){found=true;break;}
  if(!found){if(missing)missing->assign(name);return ElfStatus::unresolved_dependency;}
 }
 if(rela.seen!=relasz.seen||rela.seen!=relaent.seen||jmp.seen!=jmpsz.seen||jmp.seen!=jmptype.seen)return ElfStatus::malformed;
 if((rela.seen&&(relaent.value!=24||relasz.value%24))||(jmp.seen&&(jmptype.value!=7||jmpsz.value%24)))return ElfStatus::malformed;
 if(relasz.value>4096*24||jmpsz.value>4096*24)return ElfStatus::resource_limit;
 if((rela.seen&&!fits(rela.value,relasz.value,datasize))||(jmp.seen&&!fits(jmp.value,jmpsz.value,datasize)))return ElfStatus::malformed;
 Memory staged=machine.memory.is_sparse()?Memory(machine.memory.size(),SparsePages{machine.memory.max_pages()}):Memory(machine.memory.size(),false);
 for(const auto& s:segments){const auto addr=bias+s.address;const auto result=staged.map_detailed(addr,static_cast<std::size_t>(s.memsz),Permission::read|Permission::write);
  if(result==MapStatus::resource_limit)return ElfStatus::resource_limit;
  if(result!=MapStatus::ok)return ElfStatus::overlap;
  if(s.filesz&&!staged.write(addr,file.subspan(static_cast<std::size_t>(s.offset),static_cast<std::size_t>(s.filesz))))return ElfStatus::malformed;
 }
 auto relocate=[&](std::uint64_t table,std::uint64_t length)->ElfStatus{
  for(std::uint64_t n=0;n<length;n+=24){const auto p=static_cast<std::size_t>(data+table+n),target=le(file,p,8),info=le(file,p+8,8),addend=le(file,p+16,8);const auto kind=static_cast<std::uint32_t>(info);const auto index=info>>32;std::uint64_t value=0;
   if(kind==8&&index==0)value=bias+addend;
   else if((kind==1||kind==6||kind==7)&&index>0&&index<symsz.value/24){
    const auto pos=static_cast<std::size_t>(data+sym.value+index*24),nameoff=le(file,pos,4);const auto info_byte=file[pos+4],visibility=file[pos+5];const auto section=le(file,pos+6,2),symvalue=le(file,pos+8,8);const auto binding=info_byte>>4,stype=info_byte&15;
    if((binding!=0&&binding!=1&&binding!=2)||stype>2||visibility>3||section>=0xff00)return ElfStatus::unsupported;
    if(!section){if(binding==0||visibility!=0||nameoff>=strsz.value)return ElfStatus::malformed;
     const auto start=static_cast<std::size_t>(data+str.value+nameoff),limit=static_cast<std::size_t>(data+str.value+strsz.value);auto e=start;
     while(e<limit&&e-start<=255&&file[e])++e;
     if(e==start||e==limit||e-start>255)return ElfStatus::malformed;
     const std::string_view name(reinterpret_cast<const char*>(file.data()+start),e-start);bool found=false;
     if(!declared.empty()){
      const auto first=name.find('#'),second=first==name.npos?name.npos:name.find('#',first+1);
      if(first==name.npos||first==0||second==name.npos||second==first+1||second+1==name.size()||name.find('#',second+1)!=name.npos)return ElfStatus::malformed;
      std::uint16_t lib=0,mod=0;if(!sce_parse_id_token(name.substr(first+1,second-first-1),lib)||!sce_parse_id_token(name.substr(second+1),mod))return ElfStatus::malformed;
      bool haslib=false,hasmod=false;for(const auto& [k,v]:declared){const auto id=static_cast<std::uint16_t>(v>>48);if(k==SceDependencyKind::library&&id==lib)haslib=true;if(k==SceDependencyKind::module&&id==mod)hasmod=true;}
      if(!haslib||!hasmod)return ElfStatus::malformed;
     }
     for(const auto& item:bindings)if(item.name==name){value=item.guest_address;found=true;break;}
     if(!found){if(missing)missing->assign(name);return ElfStatus::unresolved_symbol;}
    }else{bool inside=false;for(const auto& s:segments)if(symvalue>=s.address&&fits(symvalue-s.address,1,s.memsz)){inside=true;break;}if(!inside||!add(bias,symvalue,value))return ElfStatus::malformed;}
    if(kind==1)value+=addend;
   }else return ElfStatus::unsupported;
   bool writable=false;for(const auto& s:segments)if((s.flags&2)&&target>=s.address&&fits(target-s.address,8,s.memsz)){writable=true;break;}
   std::uint64_t address=0;if(!writable||!add(bias,target,address))return ElfStatus::malformed;
   std::array<std::uint8_t,8> bytes{};for(unsigned j=0;j<8;++j)bytes[j]=static_cast<std::uint8_t>(value>>(8*j));if(!staged.write(address,bytes))return ElfStatus::malformed;
  }return ElfStatus::ok;
 };
 if(rela.seen){const auto r=relocate(rela.value,relasz.value);if(r!=ElfStatus::ok)return r;}
 if(jmp.seen){const auto r=relocate(jmp.value,jmpsz.value);if(r!=ElfStatus::ok)return r;}
 for(const auto& s:segments){Permission perm=Permission::none;if(s.flags&4)perm=perm|Permission::read;if(s.flags&2)perm=perm|Permission::write;if(s.flags&1)perm=perm|Permission::execute;if(!staged.protect(bias+s.address,static_cast<std::size_t>(s.memsz),perm))return ElfStatus::malformed;}
 const auto stack=staged.map_detailed(stack_base,stack_size,Permission::read|Permission::write);
 if(stack==MapStatus::resource_limit)return ElfStatus::resource_limit;
 if(stack!=MapStatus::ok)return ElfStatus::overlap;
 std::array<std::uint8_t,1> probe{};if(!staged.fetch(ip,probe))return ElfStatus::no_executable_entry;
 machine.memory=std::move(staged);machine.cpu.state=CpuState{};machine.cpu.state.rip=ip;*machine.cpu.state.reg(4)=stack_base+stack_size;return ElfStatus::ok;
}
}

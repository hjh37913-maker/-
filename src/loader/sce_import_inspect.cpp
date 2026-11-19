#include "loader/sce_import_inspect.h"
#include "storage/storage.h"
#include <span>
#include <system_error>
namespace nightfall::loader {
namespace {
std::uint64_t le(std::span<const std::uint8_t>b,std::size_t p,unsigned n){std::uint64_t x=0;for(unsigned i=0;i<n;++i)x|=std::uint64_t(b[p+i])<<(8*i);return x;}
bool fits(std::uint64_t p,std::uint64_t n,std::uint64_t size){return p<=size&&n<=size-p;}
}
const char* sce_inspect_status_name(SceInspectStatus s) noexcept {
 switch(s){case SceInspectStatus::ok:return "ok";case SceInspectStatus::read_error:return "read_error";case SceInspectStatus::not_elf:return "not_elf";case SceInspectStatus::unsupported:return "unsupported";case SceInspectStatus::malformed:return "malformed";case SceInspectStatus::resource_limit:return "resource_limit";}
 return "unknown";
}
SceImportReport inspect_sce_imports(const std::filesystem::path& path){
 SceImportReport out;std::error_code ec;out.file_size=std::filesystem::file_size(path,ec);if(ec)return out;
 if(out.file_size<4){out.status=SceInspectStatus::not_elf;return out;}
 auto head=storage::read_range(path,0,static_cast<std::size_t>(out.file_size<64?out.file_size:64),64);if(head.status!=storage::ReadStatus::ok)return out;
 const auto& h=head.bytes;if(h[0]!=0x7f||h[1]!='E'||h[2]!='L'||h[3]!='F'){out.status=SceInspectStatus::not_elf;return out;}
 if(h.size()<64){out.status=SceInspectStatus::malformed;return out;}
 out.osabi=h[7];out.object_type=static_cast<std::uint16_t>(le(h,16,2));out.machine=static_cast<std::uint16_t>(le(h,18,2));
 // Values are only a candidate filter observed in a public PS5 project;
 // they may also appear elsewhere and are not an authenticity check.
 if(h[4]!=2||h[5]!=1||h[6]!=1||out.osabi!=9||out.machine!=62||(out.object_type!=0xfe10&&out.object_type!=0xfe18)){out.status=SceInspectStatus::unsupported;return out;}
 if(le(h,20,4)!=1||le(h,52,2)!=64||le(h,54,2)!=56){out.status=SceInspectStatus::malformed;return out;}
 const auto phoff=le(h,32,8),count=le(h,56,2);if(!count||count==0xffff||count>128){out.status=SceInspectStatus::resource_limit;return out;}
 if(!fits(phoff,count*56,out.file_size)){out.status=SceInspectStatus::malformed;return out;}
 auto ph=storage::read_range(path,phoff,static_cast<std::size_t>(count*56),128*56);if(ph.status!=storage::ReadStatus::ok)return out;
 bool have_dyn=false,have_data=false;std::uint64_t dynoff=0,dynlen=0,dataoff=0,datalen=0;
 for(std::size_t i=0;i<count;++i){const auto p=i*56;const auto type=le(ph.bytes,p,4),off=le(ph.bytes,p+8,8),len=le(ph.bytes,p+32,8);
  if(type!=2&&type!=0x61000000)continue;
  if(!fits(off,len,out.file_size)){out.status=SceInspectStatus::malformed;return out;}
  if(type==2){if(have_dyn){out.status=SceInspectStatus::malformed;return out;}have_dyn=true;dynoff=off;dynlen=len;}
  else{if(have_data){out.status=SceInspectStatus::malformed;return out;}have_data=true;dataoff=off;datalen=len;}
 }
 if(!have_dyn||!have_data){out.status=SceInspectStatus::unsupported;return out;}
 if(!dynlen||dynlen%16){out.status=SceInspectStatus::malformed;return out;}if(dynlen>16384){out.status=SceInspectStatus::resource_limit;return out;}
 auto dynamic=storage::read_range(path,dynoff,static_cast<std::size_t>(dynlen),16384);if(dynamic.status!=storage::ReadStatus::ok)return out;
 bool ended=false,hs=false,hl=false,hy=false,he=false,hz=false;std::uint64_t st=0,sl=0,sy=0,se=0,ss=0;
 for(std::size_t p=0;p<dynlen;p+=16){const auto tag=le(dynamic.bytes,p,8),v=le(dynamic.bytes,p+8,8);if(!tag){ended=true;break;}
  bool* flag=nullptr;std::uint64_t* target=nullptr;
  switch(tag){case 0x61000035:flag=&hs;target=&st;break;case 0x61000037:flag=&hl;target=&sl;break;case 0x61000039:flag=&hy;target=&sy;break;case 0x6100003b:flag=&he;target=&se;break;case 0x6100003f:flag=&hz;target=&ss;break;default:break;}
  if(flag){if(*flag){out.status=SceInspectStatus::malformed;return out;}*flag=true;*target=v;}
 }
 if(!ended){out.status=SceInspectStatus::malformed;return out;}if(!hs||!hl||!hy||!he||!hz){out.status=SceInspectStatus::unsupported;return out;}
 if(se!=24||ss%24||!sl||!fits(st,sl,datalen)||!fits(sy,ss,datalen)){out.status=SceInspectStatus::malformed;return out;}
 if(sl>65536||ss>4096*24){out.status=SceInspectStatus::resource_limit;return out;}
 auto strings=storage::read_range(path,dataoff+st,static_cast<std::size_t>(sl),65536);
 auto symbols=storage::read_range(path,dataoff+sy,static_cast<std::size_t>(ss),4096*24);
 if(strings.status!=storage::ReadStatus::ok||symbols.status!=storage::ReadStatus::ok)return out;
 for(std::size_t i=0;i<ss/24;++i){const auto p=i*24, name=le(symbols.bytes,p,4);const auto info=symbols.bytes[p+4];const auto section=le(symbols.bytes,p+6,2);const auto binding=info>>4;
  if(section!=0||!(binding==1||binding==2))continue;
  if(name>=sl){out.imports.clear();out.status=SceInspectStatus::malformed;return out;}
  std::size_t e=static_cast<std::size_t>(name);while(e<sl&&e-name<=255&&strings.bytes[e])++e;
  if(e==name||e==sl||e-name>255){out.imports.clear();out.status=SceInspectStatus::malformed;return out;}
  out.imports.push_back({std::string(reinterpret_cast<const char*>(strings.bytes.data()+name),e-name),static_cast<std::uint32_t>(i),static_cast<std::uint8_t>(binding),static_cast<std::uint8_t>(info&15)});
 }
 out.status=SceInspectStatus::ok;return out;
}
}

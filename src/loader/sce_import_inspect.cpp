#include "loader/sce_import_inspect.h"
#include "storage/storage.h"
#include <span>
#include <system_error>
namespace nightfall::loader {
namespace {
std::uint64_t le(std::span<const std::uint8_t>b,std::size_t p,unsigned n){std::uint64_t x=0;for(unsigned i=0;i<n;++i)x|=std::uint64_t(b[p+i])<<(8*i);return x;}
bool fits(std::uint64_t p,std::uint64_t n,std::uint64_t size){return p<=size&&n<=size-p;}
// Numeric declaration IDs are represented as a compact big-endian radix-64
// token in encoded symbol names; no NID hashing or decoding is performed.
std::string id_token(std::uint16_t id){
 constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";
 std::string out;do{out.insert(out.begin(),alphabet[id%64]);id=static_cast<std::uint16_t>(id/64);}while(id);return out;
}
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
 bool ended=false,hs=false,hl=false,hy=false,he=false,hz=false;
 bool hr=false,hrsz=false,hrent=false,hj=false,hjsz=false,hjtype=false;
 std::uint64_t st=0,sl=0,sy=0,se=0,ss=0,rt=0,rs=0,re=0,jt=0,js=0,jtype=0;
 std::vector<std::pair<SceDeclarationKind,std::uint64_t>> declarations;
 for(std::size_t p=0;p<dynlen;p+=16){const auto tag=le(dynamic.bytes,p,8),v=le(dynamic.bytes,p+8,8);if(!tag){ended=true;break;}
  if(tag==0x6100000f||tag==0x61000045||tag==0x6100000d||tag==0x61000043||tag==0x61000015||tag==0x61000049||tag==0x61000013||tag==0x61000047){
   const auto kind=(tag==0x6100000f||tag==0x61000045)?SceDeclarationKind::imported_module:(tag==0x6100000d||tag==0x61000043)?SceDeclarationKind::exported_module:(tag==0x61000015||tag==0x61000049)?SceDeclarationKind::imported_library:SceDeclarationKind::exported_library;
   declarations.emplace_back(kind,v);continue;
  }
  bool* flag=nullptr;std::uint64_t* target=nullptr;
  switch(tag){case 0x61000035:flag=&hs;target=&st;break;case 0x61000037:flag=&hl;target=&sl;break;case 0x61000039:flag=&hy;target=&sy;break;case 0x6100003b:flag=&he;target=&se;break;case 0x6100003f:flag=&hz;target=&ss;break;
  case 0x6100002f:flag=&hr;target=&rt;break;case 0x61000031:flag=&hrsz;target=&rs;break;case 0x61000033:flag=&hrent;target=&re;break;
  case 0x61000029:flag=&hj;target=&jt;break;case 0x6100002d:flag=&hjsz;target=&js;break;case 0x6100002b:flag=&hjtype;target=&jtype;break;default:break;}
  if(flag){if(*flag){out.status=SceInspectStatus::malformed;return out;}*flag=true;*target=v;}
 }
 if(!ended){out.status=SceInspectStatus::malformed;return out;}if(!hs||!hl||!hy||!he||!hz){out.status=SceInspectStatus::unsupported;return out;}
 if(se!=24||ss%24||!sl||!fits(st,sl,datalen)||!fits(sy,ss,datalen)){out.status=SceInspectStatus::malformed;return out;}
 if(sl>65536||ss>4096*24){out.status=SceInspectStatus::resource_limit;return out;}
 if(hr!=hrsz||hr!=hrent||hj!=hjsz||hj!=hjtype||(hr&&(re!=24||rs%24))||(hj&&(jtype!=7||js%24))){out.status=SceInspectStatus::malformed;return out;}
 if((hr&&!fits(rt,rs,datalen))||(hj&&!fits(jt,js,datalen))){out.status=SceInspectStatus::malformed;return out;}
 if(rs>4096*24||js>4096*24){out.status=SceInspectStatus::resource_limit;return out;}
 auto strings=storage::read_range(path,dataoff+st,static_cast<std::size_t>(sl),65536);
 auto symbols=storage::read_range(path,dataoff+sy,static_cast<std::size_t>(ss),4096*24);
 if(strings.status!=storage::ReadStatus::ok||symbols.status!=storage::ReadStatus::ok)return out;
 for(const auto& [kind,raw]:declarations){const auto index=static_cast<std::uint32_t>(raw);if(index>=sl){out.declarations.clear();out.status=SceInspectStatus::malformed;return out;}
  std::size_t e=index;while(e<sl&&e-index<=255&&strings.bytes[e])++e;
  if(e==index||e==sl||e-index>255){out.declarations.clear();out.status=SceInspectStatus::malformed;return out;}
  const bool module=kind==SceDeclarationKind::imported_module||kind==SceDeclarationKind::exported_module;
  const auto major=static_cast<std::uint16_t>(module?(raw>>40)&0xff:(raw>>32)&0xffff);
  const auto minor=static_cast<std::uint16_t>(module?(raw>>32)&0xff:0);
  out.declarations.push_back({kind,std::string(reinterpret_cast<const char*>(strings.bytes.data()+index),e-index),static_cast<std::uint16_t>(raw>>48),major,minor});
 }
 for(std::size_t i=0;i<ss/24;++i){const auto p=i*24, name=le(symbols.bytes,p,4);const auto info=symbols.bytes[p+4];const auto section=le(symbols.bytes,p+6,2);const auto binding=info>>4;
  if(section!=0||!(binding==1||binding==2))continue;
  if(name>=sl){out.imports.clear();out.declarations.clear();out.status=SceInspectStatus::malformed;return out;}
  std::size_t e=static_cast<std::size_t>(name);while(e<sl&&e-name<=255&&strings.bytes[e])++e;
  if(e==name||e==sl||e-name>255){out.imports.clear();out.declarations.clear();out.status=SceInspectStatus::malformed;return out;}
  out.imports.push_back({std::string(reinterpret_cast<const char*>(strings.bytes.data()+name),e-name),static_cast<std::uint32_t>(i),static_cast<std::uint8_t>(binding),static_cast<std::uint8_t>(info&15)});
 }
 // Resolve only local qualifier tokens. Duplicate declaration IDs remain
 // unmatched. Indexed lookup keeps work bounded even with large tables.
 std::vector<int> libraries(65536,-1),modules(65536,-1);
 for(std::size_t i=0;i<out.declarations.size();++i){const auto& decl=out.declarations[i];
  std::vector<int>* table=decl.kind==SceDeclarationKind::imported_library?&libraries:decl.kind==SceDeclarationKind::imported_module?&modules:nullptr;
  if(table){auto& slot=(*table)[decl.id];slot=slot==-1?static_cast<int>(i):-2;}
 }
 auto decode_token=[](std::string_view token,int& result){
  constexpr std::string_view alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";
  if(token.empty()||token.size()>3)return false;
  unsigned value=0;for(char ch:token){const auto digit=alphabet.find(ch);if(digit==alphabet.npos)return false;value=value*64+static_cast<unsigned>(digit);}
  if(value>65535||id_token(static_cast<std::uint16_t>(value))!=token)return false;
  result=static_cast<int>(value);return true;
 };
 for(auto& item:out.imports){
  const auto first=item.encoded_name.find('#');if(first==std::string::npos||first==0)continue;
  const auto second=item.encoded_name.find('#',first+1);
  if(second==std::string::npos||second==first+1||second+1==item.encoded_name.size()||item.encoded_name.find('#',second+1)!=std::string::npos)continue;
  item.qualified=true;int lib=0,mod=0;
  if(decode_token(std::string_view(item.encoded_name).substr(first+1,second-first-1),lib)&&libraries[lib]>=0)item.library_declaration=libraries[lib];
  if(decode_token(std::string_view(item.encoded_name).substr(second+1),mod)&&modules[mod]>=0)item.module_declaration=modules[mod];
 }
 auto parse_relocations=[&](std::uint64_t offset,std::uint64_t length,bool plt){
  if(!length)return SceInspectStatus::ok;
  const auto table=storage::read_range(path,dataoff+offset,static_cast<std::size_t>(length),4096*24);
  if(table.status!=storage::ReadStatus::ok)return SceInspectStatus::read_error;
  for(std::size_t p=0;p<length;p+=24){const auto info=le(table.bytes,p+8,8);const auto index=info>>32;
   if(index>=ss/24)return SceInspectStatus::malformed;
   out.relocations.push_back({le(table.bytes,p,8),le(table.bytes,p+16,8),static_cast<std::uint32_t>(index),static_cast<std::uint32_t>(info),plt});
  }
  return SceInspectStatus::ok;
 };
 if(hr){const auto status=parse_relocations(rt,rs,false);if(status!=SceInspectStatus::ok){out.imports.clear();out.relocations.clear();out.declarations.clear();out.status=status;return out;}}
 if(hj){const auto status=parse_relocations(jt,js,true);if(status!=SceInspectStatus::ok){out.imports.clear();out.relocations.clear();out.declarations.clear();out.status=status;return out;}}
 out.status=SceInspectStatus::ok;return out;
}
}

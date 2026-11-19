#include "runtime/guest_services.h"
#include <algorithm>
#include <array>
#include <new>
#include <stdexcept>
namespace nightfall::runtime {
namespace {
constexpr std::size_t limit=65536;
std::uint64_t read64(const std::array<std::uint8_t,8>& b){std::uint64_t v=0;for(unsigned i=0;i<8;++i)v|=std::uint64_t(b[i])<<(8*i);return v;}
bool overlap(std::uint64_t a,std::uint64_t b,std::uint64_t n){return a<=b?b-a<n:a-b<n;}
}
bool GuestServices::add(std::string name,std::uint64_t address,ServiceKind kind){
 if(entries_.size()>=128||name.empty()||name.size()>255||name.find('\0')!=std::string::npos)return false;
 if(kind!=ServiceKind::copy&&kind!=ServiceKind::move&&kind!=ServiceKind::fill&&kind!=ServiceKind::compare&&kind!=ServiceKind::string_length)return false;
 for(const auto& entry:entries_)if(entry.name==name||entry.address==address)return false;
 try{entries_.push_back({std::move(name),address,kind});return true;}catch(const std::bad_alloc&){return false;}catch(const std::length_error&){return false;}
}
std::vector<loader::ImportBinding> GuestServices::bindings() const {
 std::vector<loader::ImportBinding> result;result.reserve(entries_.size());
 for(const auto& entry:entries_)result.push_back({entry.name,entry.address});
 return result;
}
ServiceStatus GuestServices::dispatch(Machine& machine) const {
 const Entry* match=nullptr;for(const auto& e:entries_)if(e.address==machine.cpu.state.rip){match=&e;break;}
 if(!match)return ServiceStatus::not_service;
 auto& s=machine.cpu.state;const auto rsp=*s.reg(4),dst=*s.reg(7),src=*s.reg(6),n=*s.reg(2);
 if(rsp>UINT64_MAX-8)return ServiceStatus::memory_fault;
 std::array<std::uint8_t,8> ret{};if(!machine.memory.read(rsp,ret))return ServiceStatus::memory_fault;
 std::uint64_t result=0;
 try{
  if(match->kind==ServiceKind::string_length){
   for(std::size_t i=0;i<=limit;++i){if(dst>UINT64_MAX-i)return ServiceStatus::memory_fault;std::array<std::uint8_t,1> c{};if(!machine.memory.read(dst+i,c))return ServiceStatus::memory_fault;if(!c[0]){result=i;break;}if(i==limit)return ServiceStatus::resource_limit;}
  }else{
   if(n>limit)return ServiceStatus::resource_limit;
   const auto count=static_cast<std::size_t>(n);
   if(match->kind==ServiceKind::copy&&n&&overlap(dst,src,n))return ServiceStatus::unsupported; // memcpy overlap is undefined
   if(match->kind==ServiceKind::copy||match->kind==ServiceKind::move||match->kind==ServiceKind::fill){
    std::vector<std::uint8_t> data(count);
    if(match->kind==ServiceKind::fill)std::fill(data.begin(),data.end(),static_cast<std::uint8_t>(src));
    else if(count&&!machine.memory.read(src,data))return ServiceStatus::memory_fault;
    if(count&&!machine.memory.write(dst,data))return ServiceStatus::memory_fault;
    result=dst;
   }else{ // compare: C specifies only the sign of the result
    std::vector<std::uint8_t> a(count),b(count);
    if(count&&(!machine.memory.read(dst,a)||!machine.memory.read(src,b)))return ServiceStatus::memory_fault;
    for(std::size_t i=0;i<count;++i)if(a[i]!=b[i]){result=a[i]<b[i]?UINT64_MAX:1;break;}
   }
  }
 }catch(const std::bad_alloc&){return ServiceStatus::resource_limit;}catch(const std::length_error&){return ServiceStatus::resource_limit;}
 // The destination may overlap the return address. A real RET reads the
 // possibly modified stack bytes after the function writes, not before.
 if(!machine.memory.read(rsp,ret))return ServiceStatus::memory_fault;
 s.rax=result;s.rip=read64(ret);*s.reg(4)=rsp+8;return ServiceStatus::completed;
}
GuestRunReport run_with_services(Machine& machine,const GuestServices& services,std::size_t budget){
 GuestRunReport report;for(std::size_t i=0;i<budget;++i){const auto rip=machine.cpu.state.rip;const auto service=services.dispatch(machine);++report.attempted;
  if(service==ServiceStatus::completed)continue;
  if(service!=ServiceStatus::not_service){report.stop_rip=rip;report.result=service==ServiceStatus::memory_fault?GuestRunResult::memory_fault:service==ServiceStatus::resource_limit?GuestRunResult::service_limit:GuestRunResult::unsupported_service;return report;}
  const auto step=machine.cpu.step(machine.memory);if(step==StepResult::running)continue;
  report.stop_rip=rip;report.result=step==StepResult::halted?GuestRunResult::halted:step==StepResult::syscall_trap?GuestRunResult::syscall_trap:step==StepResult::invalid_instruction?GuestRunResult::invalid_instruction:step==StepResult::divide_error?GuestRunResult::divide_error:step==StepResult::alignment_fault?GuestRunResult::alignment_fault:GuestRunResult::memory_fault;return report;
 }
 report.stop_rip=machine.cpu.state.rip;return report;
}
}

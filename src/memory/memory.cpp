#include "memory/memory.h"
#include <algorithm>
#include <new>
#include <stdexcept>
#include <utility>
namespace nightfall {
namespace {
constexpr std::uint8_t mapped=8;
constexpr std::uint8_t flags(Permission p) { return static_cast<std::uint8_t>(p); }
constexpr bool valid(Permission p) { return (flags(p)&~7u)==0; }
}
Memory::Memory(std::size_t size,bool map_default)
    : capacity_(size),bytes_(size,0),attributes_(size,map_default?mapped|7:0) {}
Memory::Memory(std::size_t size,SparsePages sparse)
    : capacity_(size),sparse_(true),page_limit_(sparse.max_pages) {}
Memory::Memory(Memory&& other) noexcept
    : capacity_(other.capacity_),sparse_(other.sparse_),page_limit_(other.page_limit_),
      bytes_(std::move(other.bytes_)),attributes_(std::move(other.attributes_)),pages_(std::move(other.pages_)) {
    other.capacity_=0; other.sparse_=false; other.page_limit_=0;
    other.bytes_.clear(); other.attributes_.clear(); other.pages_.clear();
}
Memory& Memory::operator=(Memory&& other) noexcept {
    if(this!=&other) {
        capacity_=other.capacity_; sparse_=other.sparse_; page_limit_=other.page_limit_;
        bytes_=std::move(other.bytes_); attributes_=std::move(other.attributes_); pages_=std::move(other.pages_);
        other.capacity_=0; other.sparse_=false; other.page_limit_=0;
        other.bytes_.clear(); other.attributes_.clear(); other.pages_.clear();
    }
    return *this;
}
bool Memory::contains(std::uint64_t address,std::size_t count) const noexcept {
    return address<=capacity_ && count<=capacity_-static_cast<std::size_t>(address);
}
bool Memory::accessible(std::uint64_t address,std::size_t count,Permission permission) const noexcept {
    if(!contains(address,count)) return false;
    const auto required=mapped|flags(permission);
    if(!sparse_) {
        auto begin=attributes_.begin()+static_cast<std::size_t>(address);
        return std::all_of(begin,begin+count,[required](auto attr){ return (attr&required)==required; });
    }
    while(count) {
        const auto page=address/page_size, offset=static_cast<std::size_t>(address%page_size), n=std::min(count,page_size-offset);
        auto it=pages_.find(page); if(it==pages_.end()) return false;
        auto begin=it->second.attributes.begin()+offset;
        if(!std::all_of(begin,begin+n,[required](auto attr){ return (attr&required)==required; })) return false;
        address+=n; count-=n;
    }
    return true;
}
MapStatus Memory::map_detailed(std::uint64_t address,std::size_t count,Permission permissions) {
    if(!count || !contains(address,count) || !valid(permissions)) return MapStatus::invalid;
    if(!sparse_) {
        auto offset=static_cast<std::size_t>(address); auto begin=attributes_.begin()+offset;
        if(!std::all_of(begin,begin+count,[](auto attr){ return !(attr&mapped); })) return MapStatus::overlap;
        std::fill_n(bytes_.begin()+offset,count,0); std::fill_n(begin,count,mapped|flags(permissions)); return MapStatus::ok;
    }
    const auto first=address/page_size, last=(address+count-1)/page_size;
    // More pages than the cap can never be mapped, even if some already exist.
    if(last-first+1>page_limit_) return MapStatus::resource_limit;
    std::size_t missing=0;
    for(auto page=first;page<=last;++page) {
        auto it=pages_.find(page); if(it==pages_.end()) { ++missing; continue; }
        const auto start=page==first?static_cast<std::size_t>(address%page_size):0;
        const auto end=page==last?static_cast<std::size_t>((address+count-1)%page_size)+1:page_size;
        if(!std::all_of(it->second.attributes.begin()+start,it->second.attributes.begin()+end,[](auto a){ return !(a&mapped); })) return MapStatus::overlap;
    }
    if(missing>page_limit_-pages_.size()) return MapStatus::resource_limit;
    // Allocate all missing pages off to the side; allocation failure changes no mappings.
    std::map<std::uint64_t,Page> staged;
    try { for(auto page=first;page<=last;++page) if(pages_.find(page)==pages_.end()) staged.try_emplace(page); }
    catch(const std::bad_alloc&) { return MapStatus::resource_limit; }
    catch(const std::length_error&) { return MapStatus::resource_limit; }
    pages_.merge(staged);
    auto pos=address; auto remaining=count;
    while(remaining) {
        auto& p=pages_.find(pos/page_size)->second; const auto off=static_cast<std::size_t>(pos%page_size), n=std::min(remaining,page_size-off);
        std::fill_n(p.bytes.begin()+off,n,0); std::fill_n(p.attributes.begin()+off,n,mapped|flags(permissions)); pos+=n; remaining-=n;
    }
    return MapStatus::ok;
}
bool Memory::unmap(std::uint64_t address,std::size_t count) {
    if(!count || !accessible(address,count,Permission::none)) return false;
    if(!sparse_) { auto off=static_cast<std::size_t>(address); std::fill_n(attributes_.begin()+off,count,0); std::fill_n(bytes_.begin()+off,count,0); return true; }
    while(count) {
        auto it=pages_.find(address/page_size); auto& p=it->second; const auto off=static_cast<std::size_t>(address%page_size), n=std::min(count,page_size-off);
        std::fill_n(p.attributes.begin()+off,n,0); std::fill_n(p.bytes.begin()+off,n,0);
        if(std::none_of(p.attributes.begin(),p.attributes.end(),[](auto a){return a&mapped;})) pages_.erase(it);
        address+=n; count-=n;
    }
    return true;
}
bool Memory::protect(std::uint64_t address,std::size_t count,Permission permissions) {
    if(!count || !valid(permissions) || !accessible(address,count,Permission::none)) return false;
    if(!sparse_) { std::fill_n(attributes_.begin()+static_cast<std::size_t>(address),count,mapped|flags(permissions)); return true; }
    while(count) {
        auto& p=pages_.find(address/page_size)->second; const auto off=static_cast<std::size_t>(address%page_size), n=std::min(count,page_size-off);
        std::fill_n(p.attributes.begin()+off,n,mapped|flags(permissions)); address+=n; count-=n;
    }
    return true;
}
bool Memory::can_access(std::uint64_t address,std::size_t count,Permission permission) const noexcept {
    return valid(permission) && accessible(address,count,permission);
}
bool Memory::copy_out(std::uint64_t address,std::span<std::uint8_t> out,Permission permission) const {
    if(!accessible(address,out.size(),permission)) return false;
    if(!sparse_) { std::copy_n(bytes_.begin()+static_cast<std::size_t>(address),out.size(),out.begin()); return true; }
    while(!out.empty()) {
        const auto& p=pages_.find(address/page_size)->second; const auto off=static_cast<std::size_t>(address%page_size), n=std::min(out.size(),page_size-off);
        std::copy_n(p.bytes.begin()+off,n,out.begin()); out=out.subspan(n); address+=n;
    }
    return true;
}
bool Memory::read(std::uint64_t address,std::span<std::uint8_t> out) const { return copy_out(address,out,Permission::read); }
bool Memory::fetch(std::uint64_t address,std::span<std::uint8_t> out) const { return copy_out(address,out,Permission::execute); }
bool Memory::write(std::uint64_t address,std::span<const std::uint8_t> data) {
    if(!accessible(address,data.size(),Permission::write)) return false;
    if(!sparse_) { std::copy(data.begin(),data.end(),bytes_.begin()+static_cast<std::size_t>(address)); return true; }
    while(!data.empty()) {
        auto& p=pages_.find(address/page_size)->second; const auto off=static_cast<std::size_t>(address%page_size), n=std::min(data.size(),page_size-off);
        std::copy_n(data.begin(),n,p.bytes.begin()+off); data=data.subspan(n); address+=n;
    }
    return true;
}
MemoryStats Memory::stats() const noexcept {
    MemoryStats out; out.capacity=size();
    auto accumulate=[&](auto attr) { if(attr&mapped) { ++out.mapped; if(attr&flags(Permission::read)) ++out.readable; if(attr&flags(Permission::write)) ++out.writable; if(attr&flags(Permission::execute)) ++out.executable; } };
    if(!sparse_) for(auto a:attributes_) accumulate(a);
    else for(const auto& [index,page]:pages_) { (void)index; for(auto a:page.attributes) accumulate(a); }
    return out;
}
}

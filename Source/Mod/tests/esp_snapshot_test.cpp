#include "../src/modules/EspSnapshot.hpp"
#include <array>
#include <cstring>
#include <cstdio>
#include <cstdlib>
using namespace utility::modules::esp_snapshot;
void check(bool ok,const char* text){if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
int main(){
    std::array<unsigned char,128> registry{},pool{};
    std::array<unsigned char,64> types{};
    std::array<unsigned char,0x260> local{},other{};
    std::array<std::uint32_t,3> packed{0x4000A,0x4000B,0xFFFC0000};
    std::array<std::uintptr_t,128> page{};
    std::array<std::uintptr_t,1> pages{reinterpret_cast<std::uintptr_t>(page.data())};
    const auto ptr=[](auto& v){return reinterpret_cast<std::uintptr_t>(v.data());};
    const auto put=[]<class T>(auto& v,std::size_t at,T value){std::memcpy(v.data()+at,&value,sizeof(value));};
    put(registry,0x68,ptr(types));put(registry,0x70,ptr(types)+32);
    put(types,8,std::uint32_t{0x85B93800});put(types,16,ptr(pool));
    put(pool,0x20,ptr(packed));put(pool,0x28,ptr(packed)+12);put(pool,0x50,ptr(pages));
    put(local,0x10,ptr(registry));put(local,0x18,packed[0]);
    put(other,0x10,ptr(registry));put(other,0x18,packed[1]);page[0]=ptr(local);page[1]=ptr(other);
    const auto read=[&](std::uintptr_t at,void* out,std::size_t n){
        bool valid=false;const auto range=[&](auto& v){const auto start=ptr(v);valid=valid||(at>=start&&at-start<=sizeof(v)&&n<=sizeof(v)-(at-start));};
        range(registry);range(pool);range(types);range(local);range(other);range(packed);range(page);range(pages);
        if(valid)std::memcpy(out,reinterpret_cast<void*>(at),n);return valid;
    };
    std::vector<Actor> out;
    check(actors(ptr(registry),ptr(local),packed[0],out,read)&&out.size()==2,"live local membership and remote entity decoded; tombstone skipped");
    put(other,0x18,std::uint32_t{0x8000B});
    check(actors(ptr(registry),ptr(local),packed[0],out,read)&&out.size()==1,"reused actor generation rejected");
    put(other,0x18,packed[1]);put(other,0x10,std::uintptr_t{123});
    check(actors(ptr(registry),ptr(local),packed[0],out,read)&&out.size()==1,"wrong registry rejected");
    check(!actors(ptr(registry),ptr(local),0,out,read)&&out.empty(),"missing validated local member fails closed");
    put(pool,0x28,ptr(packed)+13);
    check(!actors(ptr(registry),ptr(local),packed[0],out,read),"misaligned packed end rejected");
    put(pool,0x28,ptr(packed)+65540);
    check(!actors(ptr(registry),ptr(local),packed[0],out,read),"oversized pool rejected before following pages");
    put(pool,0x28,ptr(packed)+12);unsigned end_reads{};
    const auto racing=[&](std::uintptr_t at,void* dst,std::size_t n){if(!read(at,dst,n))return false;
        if(at==ptr(pool)+0x28&&++end_reads==2){const auto changed=ptr(packed)+8;std::memcpy(dst,&changed,8);}return true;};
    check(!actors(ptr(registry),ptr(local),packed[0],out,racing)&&out.empty(),"pool resized during capture rejected");
    const auto fault=[&](std::uintptr_t at,void* dst,std::size_t n){return at!=ptr(page)&&read(at,dst,n);};
    check(!actors(ptr(registry),ptr(local),packed[0],out,fault),"freed local page fails closed without native dereference");
    std::puts("PASS: read-only ESP snapshot safety");
}


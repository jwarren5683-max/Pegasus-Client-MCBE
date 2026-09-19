#pragma once
#include <cstdint>
#include <vector>
#include <limits>

namespace utility::modules::esp_snapshot {
struct Actor { std::uintptr_t pointer{}; std::uint32_t entity{}; };
// Read-only packed storage decoder. No Minecraft functions or foreign STL ABI.
// A changing pool/registry is rejected rather than publishing a mixed frame.
template<class Read>
bool actors(std::uintptr_t registry, std::uintptr_t local, std::uint32_t local_id,
            std::vector<Actor>& output, Read read) {
    output.clear();
    auto get=[&]<class T>(std::uintptr_t at,T& value){return read(at,&value,sizeof(value));};
    std::uintptr_t first{},last{},pool{};
    if(!registry||!local||registry>std::numeric_limits<std::uintptr_t>::max()-0x78||
       !get(registry+0x68,first)||!get(registry+0x70,last)||!first||last<first||
       (last-first)%32||(last-first)/32>4096)return false;
    for(auto at=first;at<last;at+=32) {
        std::uint32_t hash{};
        if(!get(at+8,hash))return false;
        if(hash==0x85B93800U){if(!get(at+16,pool))return false;break;}
    }
    std::uintptr_t begin{},end{},pages{};
    if(!pool||pool>std::numeric_limits<std::uintptr_t>::max()-0x58||
       !get(pool+0x20,begin)||!get(pool+0x28,end)||!get(pool+0x50,pages)||
       !begin||end<begin||(end-begin)%4||(end-begin)/4>16384||!pages)return false;
    const auto count=(end-begin)/4;bool found_local=false;
    std::vector<Actor> result;result.reserve(count);
    for(std::uintptr_t i=0;i<count;++i) {
        std::uint32_t packed{},actual{};std::uintptr_t page{},actor{},owner{};
        if(!get(begin+i*4,packed))return false;
        if((packed&0xFFFC0000U)==0xFFFC0000U)continue;
        if(!get(pages+(i/128)*8,page)||!page||!get(page+(i%128)*8,actor)||!actor||
           actor>std::numeric_limits<std::uintptr_t>::max()-0x260||
           !get(actor+0x10,owner)||!get(actor+0x18,actual))continue;
        if(owner!=registry||actual!=packed)continue;
        if(actor==local&&actual==local_id)found_local=true;
        result.push_back({actor,actual});
    }
    std::uintptr_t first2{},last2{},begin2{},end2{},pages2{};
    if(!found_local||!get(registry+0x68,first2)||!get(registry+0x70,last2)||
       !get(pool+0x20,begin2)||!get(pool+0x28,end2)||!get(pool+0x50,pages2)||
       first!=first2||last!=last2||begin!=begin2||end!=end2||pages!=pages2)return false;
    output=std::move(result);return true;
}
}


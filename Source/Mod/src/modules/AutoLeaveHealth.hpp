#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace utility::modules::auto_leave {
// 26.50 keeps the attributes component but grows each parallel instance to
// 136 bytes. Current health is still +0x7C; +0x70 is the default, not current.
// Key 7 was observed moving independently after the user's damage test.
template<class Read>
bool health_12650(std::uintptr_t component, float& health, Read read) noexcept {
    struct Headers { std::uintptr_t keys{},end{},values{},values_end{}; } before,after;
    const auto headers=[&](Headers& h){return read(component,&h.keys,8)&&read(component+8,&h.end,8)&&
        read(component+0x18,&h.values,8)&&read(component+0x20,&h.values_end,8);};
    if(!component||component>std::numeric_limits<std::uintptr_t>::max()-0x28||!headers(before)||
        !before.keys||before.end<=before.keys||(before.end-before.keys)%4||
        (before.end-before.keys)/4>128||!before.values||before.values_end<before.values||
        (before.values_end-before.values)%136||
        (before.values_end-before.values)/136!=(before.end-before.keys)/4)return false;
    const auto count=(before.end-before.keys)/4;
    bool found{};std::uint32_t previous{};float observed{},maximum{};
    for(std::size_t i=0;i<count;++i){std::uint32_t key{};
        if(!read(before.keys+i*4,&key,4)||!key||(i&&key<=previous))return false;
        previous=key;
        if(key==7){const auto instance=before.values+i*136;
            if(!read(instance+0x7C,&observed,4)||!read(instance+0x78,&maximum,4))return false;
            found=true;}
    }
    if(!found||!headers(after)||before.keys!=after.keys||before.end!=after.end||
        before.values!=after.values||before.values_end!=after.values_end||
        !std::isfinite(observed)||!std::isfinite(maximum)||maximum<=0||maximum>20||
        observed<0||observed>maximum)return false;
    health=observed;return true;
}
}


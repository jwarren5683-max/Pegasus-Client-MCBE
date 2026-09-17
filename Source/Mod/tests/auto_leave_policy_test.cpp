#include "../src/modules/AutoLeavePolicy.hpp"
#include "../src/modules/AutoLeaveHealth.hpp"
#include <array>
#include <cstring>
#include <cstdint>

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
void check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
}

int main() {
    using namespace utility::modules::auto_leave;
    std::array<std::uint32_t,3> keys{1,7,9};
    std::array<unsigned char,136*3> instances{};
    std::array<std::uintptr_t,5> component{reinterpret_cast<std::uintptr_t>(keys.data()),
        reinterpret_cast<std::uintptr_t>(keys.data()+keys.size()),0,
        reinterpret_cast<std::uintptr_t>(instances.data()),reinterpret_cast<std::uintptr_t>(instances.data()+instances.size())};
    const auto copy=[&](std::uintptr_t at,void* out,std::size_t size){
        for(auto range : {std::pair{reinterpret_cast<std::uintptr_t>(component.data()),sizeof(component)},
            std::pair{reinterpret_cast<std::uintptr_t>(keys.data()),sizeof(keys)},
            std::pair{reinterpret_cast<std::uintptr_t>(instances.data()),sizeof(instances)}})
            if(at>=range.first&&at-range.first<=range.second&&size<=range.second-(at-range.first)){
                std::memcpy(out,reinterpret_cast<void*>(at),size);return true;}
        return false;
    };
    float max=20,current=17.5F,default_value=20,health=-1;
    std::memcpy(instances.data()+136+0x78,&max,4);
    std::memcpy(instances.data()+136+0x7C,&current,4);
    std::memcpy(instances.data()+136+0x70,&default_value,4);
    const auto address=reinterpret_cast<std::uintptr_t>(component.data());
    check(health_12650(address,health,copy)&&health==17.5F,"136-byte instances read current health, not default");
    component[4]-=8;
    check(!health_12650(address,health,copy),"old instance stride rejected");component[4]+=8;
    keys[1]=6;check(!health_12650(address,health,copy),"missing health key rejected");keys[1]=7;
    keys[2]=7;check(!health_12650(address,health,copy),"duplicate health key rejected");keys[2]=9;
    current=NAN;std::memcpy(instances.data()+136+0x7C,&current,4);
    check(!health_12650(address,health,copy),"NaN native health rejected");
    current=21;std::memcpy(instances.data()+136+0x7C,&current,4);
    check(!health_12650(address,health,copy),"health above maximum rejected");
    check(!health_12650(1,health,copy),"unreadable component rejected");
    current=18;std::memcpy(instances.data()+136+0x7C,&current,4);
    unsigned header_reads{};
    const auto changing=[&](std::uintptr_t at,void* out,std::size_t size){
        const bool valid=copy(at,out,size);
        if(valid&&at==address&&++header_reads==2){std::uintptr_t altered=component[0]+4;std::memcpy(out,&altered,8);}
        return valid;
    };
    check(!health_12650(address,health,changing),"transitioning vector headers rejected");
    check(health_12650(address,health,copy)&&health==18,"stable health becomes readable again");
    check(clamp_hearts(-5.0F) == minimum_hearts, "threshold clamps at half a heart");
    check(clamp_hearts(50.0F) == maximum_hearts, "threshold clamps at ten hearts");
    check(clamp_hearts(3.24F) == 3.0F && clamp_hearts(3.26F) == 3.5F,
        "threshold snaps to half-heart steps");
    check(clamp_hearts(NAN) == default_hearts, "invalid threshold uses safe default");

    Gate gate;
    check(!gate.update(7.0F, 3.0F, true), "health above threshold stays connected");
    check(!gate.update(6.0F, 3.0F, true), "first low sample is confirmed before leaving");
    check(gate.update(6.0F, 3.0F, true), "second low sample triggers at exact threshold");
    check(gate.fired() && !gate.update(2.0F, 3.0F, true), "leave request is one shot");
    gate.reset();
    check(!gate.fired(), "new world or re-enable rearms the gate");
    check(!gate.update(4.0F, 3.0F, false), "disabled module never triggers");
    check(!gate.update(NAN, 3.0F, true) && !gate.update(30.0F, 3.0F, true),
        "invalid native health is rejected");
    std::puts("Auto Leave half-heart threshold, confirmation and one-shot policy passed.");
}

// Isolated patch/restore checks; no game attachment or desktop input.
#include "../src/modules/XrayModule.cpp"
#include "../src/modules/FullbrightModule.hpp"
#include <cstdlib>
#include <cstdio>
#include <limits>
using namespace utility::modules;
void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
    FullbrightModule module;
    check(module.has_value()&&module.value()==15&&module.minimum_value()==9&&module.maximum_value()==15,"slider range/default");
    check(module.value_label()=="Light level"&&module.value_suffix().empty()&&module.value_decimals()==0,"light-level label");
    module.set_value(10.7F);check(module.value()==11&&state().fullbright_level==11,"slider snaps and publishes level");
    module.set_value(-100);check(module.value()==9,"lower bound");
    module.set_value(100);check(module.value()==15,"upper bound");
    module.set_value(std::numeric_limits<float>::quiet_NaN());check(module.value()==15,"invalid input ignored");
    module.adjust_value(-1);check(module.value()==14,"whole-level adjustment");
    auto& s=state();
    s.base=reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr,0x6A00000,MEM_RESERVE,PAGE_NOACCESS));
    check(s.base!=0,"reserve synthetic image");
    for(bool native: {false,true}) {
    s.native_12650=native;s.native_lighting_verified=native;
    const auto& textures=native?texture_patches_12650:texture_patches;
    const auto& mesh=native?light_patches_12650:light_patches;
    const auto all=lighting_patches(true,15);
    for(const auto& p:all){
        const auto page=(s.base+p.rva)&~uintptr_t(4095);
        check(VirtualAlloc(reinterpret_cast<void*>(page),4096,MEM_COMMIT,PAGE_READWRITE)!=nullptr,"commit patch page");
    }
    for(const auto& p:all)std::memcpy(reinterpret_cast<void*>(s.base+p.rva),p.before.data(),p.size);
    for(int level: {15,9,12,15,14,10,11,13}){
        module.set_value(static_cast<float>(level));
        check(lights(true,true,s.fullbright_level.load()),"apply selected lighting");
        s.applied_fullbright=true;s.applied_level=level;
        check(get<unsigned char>(s.base+mesh[2].rva+1)==level&&get<unsigned char>(s.base+mesh[3].rva+2)==level,"both mesh light reads use selected level");
        for(const auto& p:textures)
            check(std::memcmp(reinterpret_cast<void*>(s.base+p.rva),(level==15?p.after:p.before).data(),p.size)==0,"lower levels retain native lookup; level fifteen keeps maximum Fullbright");
        // Changing the desired level before restoration must not change the
        // byte signature expected for the patches already in the image.
        module.set_value(level==15?9.0F:15.0F);
        check(lights(false),"restore previously applied level");
        for(const auto& p:all)check(std::memcmp(reinterpret_cast<void*>(s.base+p.rva),p.before.data(),p.size)==0,"all original instructions restored");
    }
    check(lights(true,false,9),"xray alone still applies");s.applied_fullbright=false;s.applied_level=9;
    check(get<unsigned char>(s.base+mesh[2].rva+1)==15,"xray alone retains maximum mesh light");
    check(lights(false),"xray restoration");
    // A foreign modification prevents partial application or guessed writes.
    auto* corrupted=reinterpret_cast<unsigned char*>(s.base+all.back().rva);
    *corrupted^=1;
    check(!lights(true,true,15),"signature mismatch rejects all lighting writes");
    for(size_t i=0;i+1<all.size();++i)check(std::memcmp(reinterpret_cast<void*>(s.base+all[i].rva),all[i].before.data(),all[i].size)==0,"mismatch leaves other sites untouched");
    *corrupted^=1;
    }
    s.installed=true;s.native_lighting_verified=false;
    check(!module.available(),"unverified native lighting stays unavailable");
    s.native_lighting_verified=true;
    check(module.available(),"verified native lighting becomes available");
    s.native_12650=false;s.native_lighting_verified=false;s.installed=false;
    VirtualFree(reinterpret_cast<void*>(s.base),0,MEM_RELEASE);s.base=0;
    std::puts("Fullbright slider, mesh lighting, lookup and restoration passed");
}


#include "../src/modules/XrayModule.cpp"
#include "../src/framework/ModuleManager.hpp"
#include <cstdio>
#include <cstdlib>

using namespace utility::modules;
void check(bool ok,const char* text){if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
template<class T> void put(uintptr_t address,T value){std::memcpy(reinterpret_cast<void*>(address),&value,sizeof(value));}
int native_ticks{},rebuilds{};
void __fastcall mock_tick(void*){++native_ticks;}
void __fastcall mock_rebuild(void*,bool first,bool second){check(!first&&!second,"rebuild preserves native refresh arguments");++rebuilds;}
int main(){
    // Synthetic image and registry: no Minecraft hooks, game or injection.
    std::vector<unsigned char> image(0x22000);
    auto base=reinterpret_cast<uintptr_t>(image.data());
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(image.data());dos->e_lfanew=0x80;
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(image.data()+0x80);
    nt->FileHeader.NumberOfSections=1;nt->FileHeader.SizeOfOptionalHeader=sizeof(IMAGE_OPTIONAL_HEADER64);
    auto* section=IMAGE_FIRST_SECTION(nt);section->VirtualAddress=0x1000;section->Misc.VirtualSize=0x21000;
    section->Characteristics=IMAGE_SCN_MEM_WRITE|IMAGE_SCN_MEM_READ;
    std::array<std::array<unsigned char,128>,64> objects{};
    std::array<std::array<unsigned char,112>,64> blocks{};
    std::array<std::array<unsigned char,272>,64> types{};
    std::array<uintptr_t,64> entries{};
    const uintptr_t table=base+0x300;
    for(size_t i=0;i<entries.size();++i){
        const auto object=reinterpret_cast<uintptr_t>(objects[i].data());entries[i]=object;
        const auto block=reinterpret_cast<uintptr_t>(blocks[i].data()),type=reinterpret_cast<uintptr_t>(types[i].data());
        put(object,table);put(object+8,block);put(block+0x68,type);
        put(object+16,i==0?-1:0);put(object+20,0.75F);
        const char* name=i==0?"minecraft:air":i==1?"minecraft:stone":"minecraft:diamond_ore";
        put(type+0xE8,reinterpret_cast<uintptr_t>(name));put(type+0xE8+16,std::strlen(name));put(type+0xE8+24,size_t{32});
    }
    const auto begin=reinterpret_cast<uintptr_t>(entries.data()),end=begin+sizeof(entries);
    // Candidate straddles the copied-window boundary.
    const auto registry=base+0x1000+65528;put(registry,begin);put(registry+8,end);put(registry+16,end);
    auto& s=state();s.base=base;s.image_size=image.size();s.adaptive_12650=true;
    check(discover_graphics_registry(),"bounded discovery finds a vector crossing a window boundary");
    check(s.graphics_vector==registry&&s.graphics_vtable==table,"discovery records verified registry identity");
    s.installed=true;
    utility::ModuleManager manager;
    auto owned=std::make_unique<XrayModule>();auto& module=*owned;manager.add(std::move(owned));
    module.set_enabled(true);manager.tick();
    check(s.applied&&get<int>(entries[1]+16)==-1,"adaptive path hides ordinary terrain");
    check(get<float>(entries[2]+20)==0.0F,"adaptive path removes retained-block ambient occlusion");
    // Engine/resource updates must not be overwritten by restoration.
    put(entries[3]+20,0.25F);
    module.set_enabled(false);
    check(!s.desired&&!s.applied&&s.saved.empty(),"disable restores without any subsequent enabled tick");
    check(get<int>(entries[1]+16)==0&&get<float>(entries[2]+20)==0.75F,"original shape and AO restored");
    check(get<float>(entries[3]+20)==0.25F,"restoration preserves newer engine values");
    module.set_enabled(false);check(get<int>(entries[1]+16)==0,"repeated disable is harmless");
    s.adaptive_12650=false;
    s.native_12650=true;s.coordinator_table=table;s.rebuild_target=reinterpret_cast<uintptr_t>(&mock_rebuild);s.original=&mock_tick;
    std::array<uintptr_t,4> coordinator{};coordinator[0]=table;
    module.set_enabled(true);manager.tick();
    check(!s.applied&&get<int>(entries[1]+16)==0,"native path does not mutate terrain on overlay/manager tick");
    tick_hook(coordinator.data());
    check(s.applied&&get<int>(entries[1]+16)==-1&&rebuilds==1&&native_ticks==1,"native update applies graphics and requests chunk rebuild, then forwards original");
    tick_hook(coordinator.data());check(rebuilds==2&&native_ticks==2,"second refresh pass completes without repeated application");
    tick_hook(coordinator.data());check(rebuilds==2,"stable enabled state does not rebuild every frame");
    module.set_enabled(false);check(s.applied,"disable only queues restoration for engine thread");
    tick_hook(coordinator.data());
    check(!s.applied&&get<int>(entries[1]+16)==0&&rebuilds==3,"disabled module restores and refreshes through native update");
    tick_hook(coordinator.data());check(rebuilds==4,"disable finishes second refresh pass");
    check(!utility::integration::terrain_lighting::available(),"unverified 26.50 Fullbright patches remain unavailable");
    s.graphics_vector=0;s.graphics_vtable=0;s.next_discovery=0;s.discovery_section=0;s.discovery_offset=0;
    tick_hook(coordinator.data());
    check(s.graphics_vector==registry&&native_ticks>0,"native tick recovers registry that was absent at attach without suppressing original tick");
    s.native_12650=false;
    return 0;
}


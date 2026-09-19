// Read-only exact-build probe. No hooks, injected code, or world writes.
#include <Windows.h>
#include <Psapi.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "../Source/Mod/src/integration/NavigationNativeLayout.hpp"

int main(int argc,char** argv) {
    if(argc!=2)return 1;
    HANDLE process=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,std::strtoul(argv[1],nullptr,10));
    if(!process)return 2;
    auto read=[&](std::uintptr_t at,void* out,std::size_t bytes){SIZE_T got{};return ReadProcessMemory(process,reinterpret_cast<void*>(at),out,bytes,&got)&&got==bytes;};
    std::array<HMODULE,1024> modules{};DWORD needed{};std::uintptr_t base{};
    if(K32EnumProcessModulesEx(process,modules.data(),sizeof(modules),&needed,LIST_MODULES_ALL))
        for(std::size_t i=0;i<(std::min<std::size_t>)(needed/8,modules.size());++i){wchar_t name[256]{};
            K32GetModuleBaseNameW(process,modules[i],name,256);if(!_wcsicmp(name,L"Minecraft.Windows.exe"))base=reinterpret_cast<std::uintptr_t>(modules[i]);}
    std::array<unsigned char,4096> header{};if(!base||!read(base,header.data(),header.size()))return 3;
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(header.data());
    if(dos->e_lfanew<=0||dos->e_lfanew>3000)return 4;
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(header.data()+dos->e_lfanew);
    if(nt->FileHeader.TimeDateStamp!=0x6AA482FD||nt->OptionalHeader.SizeOfImage!=0x12C01000)return 5;
    std::uint32_t health_key{};read(base+0x11DC54F4,&health_key,4);
    std::printf("Native health definition key=%X\n",health_key);
    const auto deadline=GetTickCount64()+20000;std::uintptr_t cursor{};
    std::vector<unsigned char> bytes(1024*1024);
    while(GetTickCount64()<deadline){MEMORY_BASIC_INFORMATION info{};
        if(!VirtualQueryEx(process,reinterpret_cast<void*>(cursor),&info,sizeof(info)))break;
        const auto first=reinterpret_cast<std::uintptr_t>(info.BaseAddress),last=first+info.RegionSize;
        if(last<=cursor)break;cursor=last;
        if(info.State!=MEM_COMMIT||info.Type!=MEM_PRIVATE||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(info.Protect&(PAGE_READWRITE|PAGE_WRITECOPY)))continue;
        for(auto chunk=first;chunk<last&&GetTickCount64()<deadline;chunk+=bytes.size()){
            const auto size=(std::min<std::size_t>)(bytes.size(),last-chunk);if(!read(chunk,bytes.data(),size))continue;
            for(std::size_t at=0;at+8<=size;at+=8){std::uintptr_t table{};std::memcpy(&table,bytes.data()+at,8);
                if(table!=base+0xE8E1BC0)continue;
                const auto player=chunk+at;std::uintptr_t registry{},dimension{},pool_first{},pool_last{};std::uint32_t entity{};
                if(!read(player+0x10,&registry,8)||!read(player+0x18,&entity,4)||!read(player+0x1C8,&dimension,8)||!dimension||
                    !read(registry+0x68,&pool_first,8)||!read(registry+0x70,&pool_last,8)||!pool_first||pool_last<pool_first||
                    (pool_last-pool_first)%32||(pool_last-pool_first)/32>4096)continue;
                std::printf("LOCAL player=%llX entity=%X pools=%llu\n",player,entity,(pool_last-pool_first)/32);
                unsigned candidates{};
                for(auto entry=pool_first;entry<pool_last&&GetTickCount64()<deadline;entry+=32){std::uint32_t hash{};
                    if(!read(entry+8,&hash,4))break;
                    for(std::size_t stride : {80}) {
                    auto component=utility::integration::navigation_native::component(registry,entity,hash,stride,read);
                    if(!component)continue;
                    std::uintptr_t keys{},end{},values{},values_end{};
                    if(!read(component,&keys,8)||!read(component+8,&end,8)||!read(component+0x18,&values,8)||!read(component+0x20,&values_end,8)||
                        !keys||end<=keys||(end-keys)%4||(end-keys)/4>64||!values||values_end<=values||
                        (values_end-values)%((end-keys)/4))continue;
                    const auto instance_size=(values_end-values)/((end-keys)/4);
                    if(instance_size<64||instance_size>256||instance_size%8)continue;
                    ++candidates;std::printf("ATTRIBUTES candidate hash=%08X stride=%zu instanceSize=%llu player=%llX registry=%llX entity=%X count=%llu\n",hash,stride,instance_size,player,registry,entity,(end-keys)/4);
                    for(std::size_t i=0;i<(end-keys)/4;++i){std::uint32_t key{};float value{};
                        if(!read(keys+i*4,&key,4))continue;
                        std::printf("key=%08X",key);
                        for(std::size_t offset=0x60;offset<instance_size;offset+=4)
                            if(read(values+i*instance_size+offset,&value,4)&&std::isfinite(value)&&std::abs(value)<=1000)
                                std::printf(" +%zX=%.3f",offset,value);
                        std::puts("");}
                    }
                }
                if(candidates){std::uintptr_t client{},vtable{},request{};read(player+0xD70,&client,8);read(client,&vtable,8);read(vtable+14*8,&request,8);
                    std::printf("CLIENT tableRva=%llX leaveCandidateRva=%llX (not called)\n",vtable-base,request-base);
                    CloseHandle(process);return 0;}
            }
        }
    }
    CloseHandle(process);std::puts("No structurally validated attribute candidates; no memory changed.");return 6;
}


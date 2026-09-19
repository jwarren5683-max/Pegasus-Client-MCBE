#include <Windows.h>
#include <Psapi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <array>
#include <algorithm>
#include "../Source/Mod/src/modules/EspSnapshot.hpp"
#include "../Source/Mod/src/modules/EspProjection.hpp"

int main(int argc,char** argv) {
    if(argc!=2)return 1;
    const auto pid=static_cast<DWORD>(std::strtoul(argv[1],nullptr,10));
    HANDLE process=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,pid);
    if(!process){std::printf("Read-only open failed: %lu\n",GetLastError());return 2;}
    auto read=[&](std::uintptr_t at,void* out,std::size_t bytes){SIZE_T copied{};return ReadProcessMemory(process,reinterpret_cast<void*>(at),out,bytes,&copied)&&copied==bytes;};
    std::array<HMODULE,1024> modules{};DWORD needed{};std::uintptr_t base{};
    if(K32EnumProcessModulesEx(process,modules.data(),sizeof(modules),&needed,LIST_MODULES_ALL))
        for(std::size_t i=0;i<(std::min<std::size_t>)(needed/8,modules.size());++i){wchar_t name[256]{};
            K32GetModuleBaseNameW(process,modules[i],name,256);
            if(!_wcsicmp(name,L"Minecraft.Windows.exe"))base=reinterpret_cast<std::uintptr_t>(modules[i]);
            if(wcsstr(name,L"BedrockUtility"))std::wprintf(L"Loaded mod: %ls\n",name);
        }
    if(!base){CloseHandle(process);return 3;}
    std::array<unsigned char,4096> header{};if(!read(base,header.data(),header.size()))return 4;
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(header.data());
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(header.data()+dos->e_lfanew);
    if(nt->FileHeader.TimeDateStamp!=0x6AA482FD||nt->OptionalHeader.SizeOfImage!=0x12C01000){CloseHandle(process);return 5;}
    unsigned char tick[19]{};read(base+0x1C9D140,tick,sizeof(tick));
    std::printf("Renderer tick: ");for(auto b:tick)std::printf("%02X",b);std::puts("");
    std::vector<unsigned char> window(1024*1024);const auto deadline=GetTickCount64()+15000;
    std::uintptr_t cursor{};unsigned matches{};
    while(GetTickCount64()<deadline){MEMORY_BASIC_INFORMATION info{};
        if(!VirtualQueryEx(process,reinterpret_cast<void*>(cursor),&info,sizeof(info)))break;
        const auto start=reinterpret_cast<std::uintptr_t>(info.BaseAddress),end=start+info.RegionSize;
        if(end<=cursor)break;cursor=end;
        if(info.State!=MEM_COMMIT||info.Type!=MEM_PRIVATE||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(info.Protect&(PAGE_READWRITE|PAGE_WRITECOPY)))continue;
        for(auto chunk=start;chunk<end&&GetTickCount64()<deadline;chunk+=window.size()){
            const auto size=(std::min<std::size_t>)(window.size(),end-chunk);
            if(!read(chunk,window.data(),size))continue;
            for(std::size_t at=0;at+16<=size;at+=8){std::uintptr_t table{},player{};
                std::memcpy(&table,window.data()+at,8);
                if(table!=base+0xE827560&&table!=base+0xE827600)continue;
                std::memcpy(&player,window.data()+at+8,8);
                std::uintptr_t registry{},client{},dimension{},renderer{},renderer_player{},shape{};std::uint32_t id{};
                if(!player||!read(player+0x10,&registry,8)||!read(player+0x18,&id,4)||!read(player+0xD70,&client,8))continue;
                std::vector<utility::modules::esp_snapshot::Actor> actors;
                const bool packed=utility::modules::esp_snapshot::actors(registry,player,id,actors,read);
                float view[16]{},origin[3]{},bounds[6]{};utility::modules::esp_projection::Frustum frustum{};float sx{},sy{};
                const bool camera=read(client+0x420,view,64)&&read(client+0x4A0,&frustum,64)&&
                    utility::modules::esp_projection::scales(frustum,sx,sy);
                const bool camera_origin=read(client+0x1C0,&renderer,8)&&read(renderer+0x468,&renderer_player,8)&&read(renderer_player+0x660,origin,12);
                read(player+0x1C8,&dimension,8);read(player+0x220,&shape,8);read(shape,bounds,24);
                std::uintptr_t player_table{};read(player,&player_table,8);std::printf("Player table RVA: %llX\n",player_table-base);
                std::printf("Candidate %u player=%llX registry=%llX client=%llX packed=%d actors=%zu camera=%d origin=%d dim=%llX scale=%.3f,%.3f\n",++matches,player,registry,client,packed,actors.size(),camera,camera_origin,dimension,sx,sy);
                std::printf("basis=%.3f %.3f %.3f / %.3f %.3f %.3f / %.3f %.3f %.3f origin=%.2f %.2f %.2f bounds=%.2f %.2f %.2f - %.2f %.2f %.2f\n",view[0],view[1],view[2],view[4],view[5],view[6],view[8],view[9],view[10],origin[0],origin[1],origin[2],bounds[0],bounds[1],bounds[2],bounds[3],bounds[4],bounds[5]);
                for(const auto& a:actors){struct Text{char data[16];std::size_t size,capacity;} text{};
                    if(!read(a.pointer+0x240,&text,sizeof(text))||text.size>128||!text.size||text.capacity<text.size)continue;
                    char name[129]{};
                    if(text.capacity<16)std::memcpy(name,text.data,text.size);
                    else{std::uintptr_t p{};std::memcpy(&p,text.data,8);if(!read(p,name,text.size))continue;}
                    std::printf("Actor entity=%X type=%s\n",a.entity,name);
                }
                if(packed&&client>0x100000000ULL) {
                    for(unsigned offset=0x3E0;offset<0x480;offset+=4) {
                        float matrix[16]{};if(!read(client+offset,matrix,64))continue;
                        auto dot=[&](unsigned a,unsigned b){return matrix[a]*matrix[b]+matrix[a+1]*matrix[b+1]+matrix[a+2]*matrix[b+2];};
                        if(std::abs(dot(0,0)-1)<0.02F&&std::abs(dot(4,4)-1)<0.02F&&std::abs(dot(8,8)-1)<0.02F&&
                           std::abs(dot(0,4))<0.02F&&std::abs(dot(0,8))<0.02F&&std::abs(dot(4,8))<0.02F)
                            std::printf("Camera basis candidate +%X: %.3f %.3f %.3f / %.3f %.3f %.3f / %.3f %.3f %.3f\n",offset,matrix[0],matrix[1],matrix[2],matrix[4],matrix[5],matrix[6],matrix[8],matrix[9],matrix[10]);
                    }
                    for(unsigned offset=0x480;offset<0x600;offset+=4){utility::modules::esp_projection::Frustum f{};float x{},y{};
                        if(read(client+offset,&f,64)&&utility::modules::esp_projection::scales(f,x,y))std::printf("Frustum candidate +%X scale %.3f %.3f\n",offset,x,y);
                    }
                    for(unsigned offset=0x1A0;offset<=0x1F0;offset+=8){std::uintptr_t r{};
                        if(!read(client+offset,&r,8)||r<0x100000000ULL)continue;
                        for(unsigned p=0x440;p<=0x490;p+=8){std::uintptr_t rp{};
                            if(!read(r+p,&rp,8)||rp<0x100000000ULL)continue;
                            for(unsigned o=0x640;o<=0x690;o+=4){float xyz[3]{};
                                if(read(rp+o,xyz,12)&&std::abs(xyz[0]-bounds[0])<1&&std::abs(xyz[1]-bounds[1]-1.62F)<1&&std::abs(xyz[2]-bounds[2])<1)
                                    std::printf("Origin candidate client+%X renderer+%X player+%X = %.3f %.3f %.3f\n",offset,p,o,xyz[0],xyz[1],xyz[2]);
                            }
                        }
                    }
                }
                if(packed&&camera&&camera_origin){
                    std::uintptr_t region{},rt{},source{},st{},head{},node{},count{},block_fn{},chunk_fn{},source_fn{};
                    read(dimension+0xF0,&region,8);read(region,&rt,8);read(region+0x28,&source,8);read(source,&st,8);
                    read(rt+0x10,&block_fn,8);read(rt+0x148,&chunk_fn,8);read(st+0x18,&source_fn,8);
                    read(source+0x78,&head,8);read(source+0x80,&count,8);read(head,&node,8);
                    short min_y{},max_y{};read(region+0x3A,&min_y,2);read(region+0x38,&max_y,2);
                    std::printf("Storage region=%llX source=%llX blockRva=%llX chunkRva=%llX sourceRva=%llX map=%llX count=%llu heights=%d,%d\n",region,source,block_fn-base,chunk_fn-base,source_fn-base,head,count,min_y,max_y);
                    for(unsigned n=0;n<count&&n<4096&&node&&node!=head;++n){std::uintptr_t next{},chunk{},actors_head{},actors_count{},an{};int coords[2]{};
                        if(!read(node,&next,8)||!read(node+0x18,&chunk,8)||!read(node+0x10,coords,8))break;
                        read(chunk+0x13A0,&actors_head,8);read(chunk+0x13A8,&actors_count,8);
                        if(actors_count&&actors_count<65536&&read(actors_head,&an,8)) {
                            std::printf("Chunk %d,%d actor map count=%llu\n",coords[0],coords[1],actors_count);
                            for(unsigned j=0;j<actors_count&&j<12&&an&&an!=actors_head;++j){std::uintptr_t a{},vt{},a_next{};int pos[3]{};float box[6]{};unsigned key{};
                                if(!read(an,&a_next,8)||!read(an+0x18,&a,8))break;
                                read(an+0x10,&key,4);read(a,&vt,8);read(a+8,pos,12);read(a+0x50,box,24);
                                std::printf("BlockActor vtRva=%llX key=%X pos=%d,%d,%d box=%.1f %.1f %.1f - %.1f %.1f %.1f\n",vt-base,key,pos[0],pos[1],pos[2],box[0],box[1],box[2],box[3],box[4],box[5]);an=a_next;
                            }
                        }
                        node=next;
                    }
                    CloseHandle(process);return 0;
                }
                if(matches>=12){CloseHandle(process);return 6;}
            }
        }
    }
    std::printf("Probe finished: %u candidates. No game memory changed.\n",matches);CloseHandle(process);return 7;
}


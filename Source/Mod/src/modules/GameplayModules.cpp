#include "GameplayModules.hpp"
#include "EspProjection.hpp"
#include "EspSnapshot.hpp"
#include "TriggerbotPolicy.hpp"
#include "AutoLeavePolicy.hpp"
#include "AutoLeaveHealth.hpp"
#include "AutoBridgePolicy.hpp"
#include "AirjumpPolicy.hpp"
#include "JetpackPolicy.hpp"
#include "ChestEspContainers.hpp"
#include "../integration/GameContext.hpp"
#include "../integration/WorldSeed.hpp"
#include "../integration/NavigationBridge.hpp"
#include "../integration/ServerSafety.hpp"
#include "../integration/NavigationNativeLayout.hpp"
#include "../integration/BedrockBuild.hpp"
#include "../framework/Logger.hpp"
#include <TlHelp32.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace utility::modules {
namespace {
using Byte = unsigned char;
using esp_projection::Frustum;
struct Vec3 { float x{}, y{}, z{}; };
struct Box { Vec3 lower, upper; COLORREF color{}; bool player{}; Vec3 movement{}; unsigned storage_kind{}; };
struct Frame { std::vector<Box> boxes; Vec3 origin; float view[16]{}; float scale_x{}, scale_y{}; double time{}; void* player{}; void* dimension{}; };
std::atomic_bool flags[static_cast<unsigned>(GameplayFeature::count)]{};
std::atomic_bool players_only{}, ready{}, attempted{};
std::atomic<unsigned> pending_keys{};
std::atomic<unsigned> trigger_targets{triggerbot::all_targets};
std::atomic_bool trigger_ready{},airjump_ready{};
std::atomic_bool esp_ready{};
std::atomic_bool chest_ready{},chest_snapshot_logged{},chest_draw_logged{};
std::atomic_bool esp_snapshot_logged{},esp_draw_logged{};
airjump::Requests airjump_requests;
std::atomic<float> jetpack_speed{jetpack::default_speed};
std::atomic<unsigned> jetpack_revision{};
std::atomic_bool jetpack_ready{};
std::atomic<unsigned> trigger_revision{};
std::atomic<float> auto_leave_hearts{auto_leave::default_hearts};
std::atomic<unsigned> auto_leave_revision{};
std::atomic_bool auto_leave_ready{};
std::atomic_bool auto_bridge_ready{};
auto_leave::Gate auto_leave_gate;
class DeathPositionTracker final {
public:
    void reset() noexcept { player_=nullptr;dimension_=nullptr;was_alive_=false;last_alive_={}; }
    bool update(void* player,void* dimension,bool health_valid,float health,
        bool position_valid,Vec3 position,Vec3& death) noexcept {
        if(player!=player_||dimension!=dimension_) {
            player_=player;dimension_=dimension;was_alive_=false;last_alive_={};
        }
        if(!health_valid) return false;
        if(health>0.0F) {
            if(position_valid) { last_alive_=position;was_alive_=true; }
            return false;
        }
        if(!was_alive_) return false;
        was_alive_=false;
        death=last_alive_;
        return true;
    }
private:
    void* player_{};
    void* dimension_{};
    bool was_alive_{};
    Vec3 last_alive_{};
};
DeathPositionTracker death_position_tracker_12650;
class StartupNoticeGate final {
public:
    void arm() noexcept {
        unsigned expected=disarmed;
        state_.compare_exchange_strong(expected,pending);
    }
    void disarm() noexcept { state_.store(disarmed); }
    template<class Sender> bool try_send(void* player, Sender&& sender) noexcept {
        if (!player) return false;
        unsigned expected=pending;
        if (!state_.compare_exchange_strong(expected,sending)) return false;
        bool ok=false;
        try { ok=static_cast<bool>(sender(player)); } catch (...) {}
        expected=sending;
        state_.compare_exchange_strong(expected,ok?sent:pending);
        return ok;
    }
    [[nodiscard]] bool was_sent() const noexcept { return state_.load()==sent; }
private:
    enum : unsigned { disarmed, pending, sending, sent };
    std::atomic<unsigned> state_{disarmed};
};
constexpr char startup_notice_text[]="[Loki] Loaded";
StartupNoticeGate startup_notice;
unsigned key_bit(unsigned code) {
    switch(code) { case 'W': return 1; case 'S': return 2; case 'A': return 4; case 'D': return 8; case VK_SPACE: return 16; default: return 0; }
}
std::atomic<std::uint64_t> local_unique_id{};
std::mutex frame_mutex;
Frame frame,storage_frame;
std::atomic<unsigned> storage_types{chest_esp::all_types};
Byte* image{};
using Tick = void(__fastcall*)(void*);
using Immune = bool(__fastcall*)(void*, void*);
using StartMining = bool(__fastcall*)(void*, const void*, unsigned char, bool*);
using ContinueMining = bool(__fastcall*)(void*, const void*, unsigned char, const void*, bool*);
using Eject = void(__fastcall*)(void*,void*,void*,void*,void*,void*,void*,bool,void*);
Eject original_eject{};
std::atomic<void*> local_state{};
Tick original_tick{};
Tick original_server_tick{};
Immune original_immune{};
StartMining original_start{}, original_creative_start{};
ContinueMining original_continue{};
using ActorList = std::vector<void*>(__fastcall*)(void*);
ActorList runtime_actor_list{};

template<class T> T read(const void* object, std::size_t offset = 0) {
    T result{};
    if (object && integration::readable_game_memory(static_cast<const Byte*>(object) + offset, sizeof(T)))
        std::memcpy(&result, static_cast<const Byte*>(object) + offset, sizeof(T));
    return result;
}
void release_trigger_mouse();
void auto_bridge_tick(void* player);

bool on(GameplayFeature feature) {
    if(integration::server_safety::remote_session() && feature!=GameplayFeature::auto_leave&&feature!=GameplayFeature::esp&&feature!=GameplayFeature::chest_esp) return false;
    if(integration::navigation_owns_controls.load() &&
       (feature==GameplayFeature::autotool||feature==GameplayFeature::phase||feature==GameplayFeature::airjump||
        feature==GameplayFeature::autosprint||feature==GameplayFeature::triggerbot||feature==GameplayFeature::jetpack||
        feature==GameplayFeature::auto_bridge))return false;
    return flags[static_cast<unsigned>(feature)].load();
}

bool current_health(void* player,float& health) {
    const auto copy=[](std::uintptr_t at,void* output,std::size_t size) {
        if(!integration::readable_game_memory(reinterpret_cast<void*>(at),size))return false;
        std::memcpy(output,reinterpret_cast<void*>(at),size);return true;
    };
    if(integration::is_release_12650(integration::current_bedrock_build())) {
        const auto safe_copy=[](std::uintptr_t at,void* output,std::size_t size) {
            SIZE_T got{};return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),output,size,&got)&&got==size;
        };
        // Resolve the component with RPM as well; do not dereference a pool
        // while a world transition may be retiring it.
        std::uintptr_t registry{};std::uint32_t entity{};
        std::uint32_t health_key{};
        if(!safe_copy(reinterpret_cast<std::uintptr_t>(image)+0x11DC54F4,&health_key,4)||health_key!=7)return false;
        if(!player||!safe_copy(reinterpret_cast<std::uintptr_t>(player)+0x10,&registry,8)||
            !safe_copy(reinterpret_cast<std::uintptr_t>(player)+0x18,&entity,4))return false;
        const auto current=integration::navigation_native::component(registry,entity,0xFD3B0613,80,safe_copy);
        return auto_leave::health_12650(current,health,safe_copy);
    }
    const auto attributes=integration::navigation_native::component(
        read<std::uintptr_t>(player,0x10),read<std::uint32_t>(player,0x18),
        integration::navigation_native::attributes_hash,
        integration::navigation_native::attributes_size,copy);
    const auto key=read<std::uint32_t>(image,integration::navigation_native::health_definition+4);
    return key && integration::navigation_native::attribute(attributes,key,health,copy) &&
        std::isfinite(health) && health>=0.0F && health<=20.0F;
}

bool executable_game_code(const void* address) {
    MEMORY_BASIC_INFORMATION information{};
    if(!address||!VirtualQuery(address,&information,sizeof(information))||
       information.State!=MEM_COMMIT||information.Type!=MEM_IMAGE||information.AllocationBase!=image)return false;
    constexpr DWORD executable=PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
    return (information.Protect&executable)!=0 && (information.Protect&PAGE_GUARD)==0;
}

// Current 26.50 IBlockSource ABI: the already-verified getChunk(ChunkPos)
// slot is followed by getLevel(), getILevel(), then getLevelSeed64().
constexpr std::size_t block_source_get_chunk_slot_12650=0x148;
constexpr std::size_t block_source_get_seed_slot_12650=0x160;
static_assert(block_source_get_seed_slot_12650==block_source_get_chunk_slot_12650+3*sizeof(void*));
struct LevelSeed64Abi { std::uint64_t value; };
static_assert(sizeof(LevelSeed64Abi)==sizeof(std::uint64_t));

bool seed_looks_like_process_pointer(std::uint64_t value) noexcept {
    if(value<0x10000ULL||value>0x00007FFFFFFFFFFFULL)return false;
    MEMORY_BASIC_INFORMATION information{};
    return VirtualQuery(reinterpret_cast<void*>(static_cast<std::uintptr_t>(value)),&information,sizeof(information)) &&
        information.State==MEM_COMMIT;
}

void refresh_world_seed_12650(void* player) noexcept {
    static void* previous_player{};
    static void* previous_dimension{};
    static void* rejected_dimension{};
    auto* dimension=read<void*>(player,0x1C8);
    if(player!=previous_player||dimension!=previous_dimension) {
        integration::reset_world_seed();
        previous_player=player;
        previous_dimension=dimension;
        rejected_dimension=nullptr;
    }
    if(!player||!dimension||!image||rejected_dimension==dimension) return;
    std::int64_t cached{};
    if(integration::current_world_seed(cached)) return;
    const auto* profile=chest_esp::profile(image);
    if(profile!=&chest_esp::profile_12650||!chest_esp::profile_verified(image)) return;
    auto* region=read<void*>(dimension,0xF0);
    auto* table=read<void**>(region);
    if(!region||!table||read<void*>(table,block_source_get_chunk_slot_12650)!=image+profile->get_chunk) return;
    using GetSeed=LevelSeed64Abi(__fastcall*)(void*);
    const auto get_seed=read<GetSeed>(table,block_source_get_seed_slot_12650);
    if(!executable_game_code(reinterpret_cast<void*>(get_seed))) return;
    std::uint64_t seed{};
#if defined(_MSC_VER)
    __try { seed=get_seed(region).value; }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        rejected_dimension=dimension;
        Logger::instance().info("World seed unavailable: BlockSource seed accessor faulted; disabled for this dimension.");
        return;
    }
#else
    seed=get_seed(region).value;
#endif
    if(seed_looks_like_process_pointer(seed)) {
        rejected_dimension=dimension;
        Logger::instance().info("World seed unavailable: BlockSource seed accessor returned a process pointer; disabled for this dimension.");
        return;
    }
    integration::publish_world_seed(seed);
}

// IClientInstance::requestLeaveGameAsync is vtable slot 14 in the supported
// Bedrock interface. It schedules the normal save/disconnect lifecycle and
// returns immediately; it does not terminate Minecraft or forge packets.
bool request_leave_game_async(void* player) noexcept {
    using RequestLeave=void(__fastcall*)(void*);
    auto* client=read<void*>(player,0xD70);
    auto* table=read<void**>(client);
    auto request=read<RequestLeave>(table,14*sizeof(void*));
    if(integration::is_release_12650(integration::current_bedrock_build())) {
        // Exact ClientInstance vtable and method; its native diagnostic names
        // requestLeaveGameAsync. No arbitrary executable slot is accepted.
        const unsigned char expected[]{0x55,0x56,0x57,0x48,0x81,0xEC,0x00,0x01,0x00,0x00,0x48,0x8D,0xAC,0x24,0x80,0x00,0x00,0x00};
        if(!auto_leave_ready.load()||table!=reinterpret_cast<void**>(image+0xE9731B0)||
            reinterpret_cast<Byte*>(request)!=image+0x5DA3B00||
            std::memcmp(image+0x5DA3B00,expected,sizeof(expected)))return false;
    }
    if(!client||!table||!executable_game_code(read<void*>(table))||
       !executable_game_code(reinterpret_cast<void*>(request)))return false;
#if defined(_MSC_VER)
    __try { request(client); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
#else
    request(client);
#endif
    return true;
}

void auto_leave_tick(void* player,bool new_player) {
    static unsigned revision{};
    const auto current_revision=auto_leave_revision.load();
    if(new_player||revision!=current_revision){auto_leave_gate.reset();revision=current_revision;}
    float health{};
    const bool valid=current_health(player,health);
    if(!auto_leave_gate.update(health,auto_leave_hearts.load(),on(GameplayFeature::auto_leave)&&valid))return;
    release_trigger_mouse();pending_keys=0;airjump_requests.reset();
    char message[160]{};
    std::snprintf(message,sizeof(message),"Auto Leave requested at %.1f hearts (threshold %.1f).",
        static_cast<double>(health/2.0F),static_cast<double>(auto_leave_hearts.load()));
    Logger::instance().info(message);
    if(!request_leave_game_async(player))Logger::instance().info("Auto Leave failed closed: leave-game interface validation failed.");
}
template<class Function> Function native(std::uintptr_t rva) { return reinterpret_cast<Function>(image + rva); }
ActorList find_runtime_actor_list() {
    // Current open-source Bedrock SDK signature. Wildcards cover stack size and
    // spill offsets, while the argument setup makes the match specific.
    constexpr int pattern[]{0x48,0x89,0x5C,0x24,-1,0x55,0x56,0x57,0x48,0x83,0xEC,-1,
        0x48,0x8B,0xF2,0x48,0x89,0x54,0x24,-1,0x33,0xD2};
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(image+dos->e_lfanew);
    const auto* sections=IMAGE_FIRST_SECTION(nt);Byte* match{};unsigned matches{};
    for(unsigned s=0;s<nt->FileHeader.NumberOfSections;++s){const auto& section=sections[s];
        if(!(section.Characteristics&IMAGE_SCN_MEM_EXECUTE))continue;
        const auto size=section.Misc.VirtualSize;auto* begin=image+section.VirtualAddress;
        for(std::size_t at=0;at+std::size(pattern)<=size;++at){bool same=true;
            for(std::size_t i=0;i<std::size(pattern);++i)if(pattern[i]>=0&&begin[at+i]!=pattern[i]){same=false;break;}
            if(same){match=begin+at;++matches;if(matches>1)return nullptr;}
        }
    }
    return matches==1?reinterpret_cast<ActorList>(match):nullptr;
}
template<class Function> Function method(void* object, std::size_t slot) {
    return read<Function>(read<void*>(object), slot);
}
bool valid(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
Vec3 minus(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
float dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
std::string foreign_string(const void* object) {
    const auto size = read<std::size_t>(object,16), capacity = read<std::size_t>(object,24);
    if (size == 0 || size > 256 || capacity < size || capacity > 65536) return {};
    const auto* data = capacity >= 16 ? read<const char*>(object) : static_cast<const char*>(object);
    return integration::readable_game_memory(data,size) ? std::string(data,size) : std::string{};
}
bool controls_active() {
    DWORD process{}; GetWindowThreadProcessId(GetForegroundWindow(), &process);
    CURSORINFO cursor{sizeof(cursor)};
    return process == GetCurrentProcessId() && GetCursorInfo(&cursor) && !(cursor.flags & CURSOR_SHOWING);
}
bool key(int code) { return (GetAsyncKeyState(code) & 0x8000) != 0; }

// Keep controller support optional and runtime-resolved. Bedrock ships with
// different XInput DLL names across Windows builds, so no import-library or
// registry change is needed. A controller only supplies the same boolean gates
// as the keyboard path; movement and camera remain entirely user-controlled.
struct ControllerGamepad { unsigned short buttons{}; unsigned char left_trigger{},right_trigger{}; short lx{},ly{},rx{},ry{}; };
struct ControllerState { unsigned long packet{}; ControllerGamepad gamepad{}; };
using XInputGetStateFn=unsigned long(__stdcall*)(unsigned,ControllerState*);
XInputGetStateFn xinput_get_state() noexcept {
    static std::once_flag once;static XInputGetStateFn function{};
    std::call_once(once,[] {
        constexpr wchar_t names[][24]{L"xinput1_4.dll",L"xinput1_3.dll",L"xinput9_1_0.dll"};
        for(const auto& name:names) {
            if(auto library=LoadLibraryW(name)) {
                function=reinterpret_cast<XInputGetStateFn>(GetProcAddress(library,"XInputGetState"));
                if(function)return;
            }
        }
    });
    return function;
}
struct ControllerControls { bool connected{},modifier{},forward{},jump{}; };
ControllerControls controller_controls() noexcept {
    ControllerControls result{};const auto get=xinput_get_state();if(!get)return result;
    constexpr unsigned short a=0x1000,left_shoulder=0x0100,right_shoulder=0x0200;
    constexpr short forward_threshold=12000;
    for(unsigned index=0;index<4;++index) {
        ControllerState state{};
        if(get(index,&state)!=0)continue;
        result.connected=true;
        result.modifier=result.modifier||(state.gamepad.buttons&(left_shoulder|right_shoulder))!=0||state.gamepad.right_trigger>=180;
        result.forward=result.forward||state.gamepad.ly>=forward_threshold;
        result.jump=result.jump||(state.gamepad.buttons&a)!=0;
    }
    return result;
}
bool local_player(void* player) { return read<void*>(player) == image + 0xE820EC0; }
std::uint64_t unique_id(void* player) {
    std::uint64_t id{};
    native<void*(__fastcall*)(void*,std::uint64_t*)>(0x18C29B0)(player,&id);
    return id;
}
bool our_player(void* player) {
    if (local_player(player)) return true;
    return read<void*>(player) == image + 0xE833530 && local_unique_id.load() != 0 &&
        unique_id(player) == local_unique_id.load();
}

// The registry's packed ActorOwnerComponent storage owns each live actor.
// Capture entities on the native player tick; drawing uses copied bounds.
void* pool_for(void* registry, std::uint32_t type) {
    const auto begin=read<std::uintptr_t>(registry,0x68), end=read<std::uintptr_t>(registry,0x70);
    if (!begin || end < begin || end-begin > 32*4096 || !integration::readable_game_memory(reinterpret_cast<void*>(begin),end-begin)) return nullptr;
    for (auto at=begin;at<end;at+=32)
        if (read<std::uint32_t>(reinterpret_cast<void*>(at),8)==type) return read<void*>(reinterpret_cast<void*>(at),16);
    return nullptr;
}
// ClientInstance stores the camera basis as contiguous rows. Its side planes
// are transformed in-place during rendering; do not assume their coordinate space.
Vec3 camera_space(const Frame& camera, Vec3 point) {
    const auto p=minus(point,camera.origin);
    return {p.x*camera.view[0]+p.y*camera.view[1]+p.z*camera.view[2],
            p.x*camera.view[4]+p.y*camera.view[5]+p.z*camera.view[6],
            -(p.x*camera.view[8]+p.y*camera.view[9]+p.z*camera.view[10])};
}
bool configure_projection(Frame& camera,const Frustum& frustum) {
    if(!valid(camera.origin))return false;
    const Vec3 right{camera.view[0],camera.view[1],camera.view[2]};
    const Vec3 up{camera.view[4],camera.view[5],camera.view[6]};
    const Vec3 back{camera.view[8],camera.view[9],camera.view[10]};
    if(!valid(right)||!valid(up)||!valid(back)||
       std::abs(dot(right,right)-1)>0.02F||std::abs(dot(up,up)-1)>0.02F||
       std::abs(dot(back,back)-1)>0.02F||std::abs(dot(right,up))>0.02F||
       std::abs(dot(right,back))>0.02F||std::abs(dot(up,back))>0.02F)return false;
    return esp_projection::scales(frustum,camera.scale_x,camera.scale_y);
}
double esp_time() {
    static const double frequency=[] { LARGE_INTEGER value{};QueryPerformanceFrequency(&value);return static_cast<double>(value.QuadPart); }();
    LARGE_INTEGER value{};QueryPerformanceCounter(&value);return static_cast<double>(value.QuadPart)/frequency;
}
// The overlay never follows entity storage. Only this small camera snapshot is
// read between ticks; ReadProcessMemory fails safely if a world is being freed.
bool camera_read(const void* object,std::size_t offset,void* output,std::size_t size) {
    SIZE_T copied{};
    const auto address=reinterpret_cast<std::uintptr_t>(object);
    if(!address||address+offset<address||address+offset+size<address+offset)return false;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address+offset),
        output,size,&copied) && copied==size;
}
struct CameraSample { float view[16]{}; Vec3 origin{}; Frustum frustum{}; void* dimension{}; };
bool camera_sample(void* player,CameraSample& sample,const integration::BedrockBuildInfo& build=integration::current_bedrock_build()) {
    void* table{};void* client{};void* renderer{};void* renderer_player{};
    const std::size_t shift=integration::is_release_12650(build)?8:0;
    return camera_read(player,0,&table,sizeof(table)) &&
        (table==image+0xE820EC0 ||
         (integration::is_release_12650(build)&&player==integration::current_player()&&table==image+0xE8E1BC0)) &&
        camera_read(player,0x1C8,&sample.dimension,sizeof(sample.dimension)) && sample.dimension &&
        camera_read(player,0xD70,&client,sizeof(client)) &&
        camera_read(client,0x418+shift,sample.view,sizeof(sample.view)) &&
        camera_read(client,0x498+shift,&sample.frustum,sizeof(sample.frustum)) &&
        camera_read(client,0x1B8+shift,&renderer,sizeof(renderer)) &&
        camera_read(renderer,0x468,&renderer_player,sizeof(renderer_player)) &&
        camera_read(renderer_player,0x660,&sample.origin,sizeof(sample.origin));
}
bool refresh_camera(Frame& target) {
    // Retry a concurrent renderer update instead of mixing two camera frames.
    for(int attempt=0;attempt<3;++attempt) {
        CameraSample first{},second{};
        if(!camera_sample(target.player,first)||!camera_sample(target.player,second))return false;
        if(std::memcmp(first.view,second.view,sizeof(first.view)) ||
           std::memcmp(&first.origin,&second.origin,sizeof(Vec3)) ||
           std::memcmp(&first.frustum,&second.frustum,sizeof(Frustum)) || first.dimension!=second.dimension)continue;
        if(second.dimension!=target.dimension)return false;
        std::memcpy(target.view,second.view,sizeof(target.view));target.origin=second.origin;
        return configure_projection(target,second.frustum);
    }
    return false;
}
// ActorOwnerComponent uses in-place deletion: packed entries include tombstones,
// and their payload slots may still contain pointers to freed/reused actors.
bool live_actor_slot(std::uint32_t packed, std::uint32_t actual,void* registry,void* expected_registry) {
    constexpr std::uint32_t version_mask=0xFFFC0000U; // this build: 18 entity bits
    return (packed&version_mask)!=version_mask && packed==actual &&
        registry && registry==expected_registry;
}
struct CameraCache {
    float view[16]{};
    Vec3 origin{};
    float scale_x{},scale_y{};
    void* dimension{};
    void* player{};
    double time{};
    bool present{};
};
CameraCache last_camera,last_storage_camera;
bool cached_camera(Frame& target,CameraCache& cache,double now) {
    if(refresh_camera(target)) {
        std::memcpy(cache.view,target.view,sizeof(target.view));
        cache.origin=target.origin;cache.dimension=target.dimension;
        cache.scale_x=target.scale_x;cache.scale_y=target.scale_y;
        cache.player=target.player;cache.time=now;cache.present=true;return true;
    }
    // One racing sample must not blank all boxes. Retain at most one game tick,
    // and never retain a camera across a player or dimension change.
    void* current_dimension{};
    if(!cache.present||cache.player!=target.player||cache.dimension!=target.dimension||
       now<cache.time||now-cache.time>0.05||
       !camera_read(target.player,0x1C8,&current_dimension,sizeof(current_dimension))||
       current_dimension!=target.dimension) {cache.present=false;return false;}
    std::memcpy(target.view,cache.view,sizeof(target.view));target.origin=cache.origin;
    target.scale_x=cache.scale_x;target.scale_y=cache.scale_y;return true;
}
bool esp_identifier(std::string_view name) {
    const auto colon=name.find(':');
    if(colon==std::string_view::npos||colon==0||colon+1==name.size())return false;
    return std::all_of(name.begin(),name.end(),[](unsigned char c){
        return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c==':'||c=='_'||c=='-'||c=='.'||c=='/';
    });
}
bool esp_entity(void* actor,void* player,void* actor_dimension,void* dimension,std::string_view name,const Box& box) {
    return actor && actor!=player && dimension && actor_dimension==dimension && esp_identifier(name) &&
        valid(box.lower)&&valid(box.upper)&&box.upper.x>box.lower.x&&
        box.upper.y>box.lower.y&&box.upper.z>box.lower.z;
}
Vec3 interpolated_offset(const Box& box,double age) {
    const float remaining=1.0F-static_cast<float>(std::clamp(age/0.05,0.0,1.0));
    return {-box.movement.x*remaining,-box.movement.y*remaining,-box.movement.z*remaining};
}
void capture_esp(void* player) {
    Frame next;next.player=player;next.dimension=read<void*>(player,0x1C8);
    // Publish empty snapshots too, so invalid/world-transition data cannot leave ghosts.
    struct Publish { Frame& value; ~Publish(){value.time=esp_time();std::lock_guard lock(frame_mutex);frame=std::move(value);} } publish{next};
    if(!next.dimension)return;
    auto* registry=read<void*>(player,0x10);
    auto* pool=pool_for(registry,0x85B93800);
    const auto begin=read<std::uintptr_t>(pool,0x20), end=read<std::uintptr_t>(pool,0x28);
    if (!begin || end<begin || (end-begin)/4>16384) return;
    const auto count=(end-begin)/4;
    auto* pages=read<void*>(pool,0x50);
    next.boxes.reserve(count);
    for (std::size_t index=0;index<count;++index) {
        const auto packed=read<std::uint32_t>(reinterpret_cast<void*>(begin),index*4);
        if((packed&0xFFFC0000U)==0xFFFC0000U)continue;
        auto* page=read<void*>(pages,(index/128)*8);
        auto* actor=read<Byte*>(page,(index%128)*8);
        if (!actor || actor==player || !integration::readable_game_memory(actor,0x260)) continue;
        if(!live_actor_slot(packed,read<std::uint32_t>(actor,0x18),read<void*>(actor,0x10),registry))continue;
        auto* shape=read<void*>(actor,0x220);
        Box box{read<Vec3>(shape),read<Vec3>(shape,12)};
        const auto name=foreign_string(actor+0x240);
        if(!shape || !esp_entity(actor,player,read<void*>(actor,0x1C8),next.dimension,name,box))continue;
        box.player=name=="minecraft:player" || name.starts_with("minecraft:player.");
        box.color=box.player ? RGB(255,50,50) : name.starts_with("minecraft:") ? RGB(55,135,255) : RGB(60,235,90);
        auto* state=read<void*>(actor,0x218);
        if(integration::readable_game_memory(state,24)) {
            const auto movement=minus(read<Vec3>(state),read<Vec3>(state,12));
            // Interpolate ordinary tick motion, never sweep a teleport across the world.
            if(valid(movement)&&dot(movement,movement)<16.0F)box.movement=movement;
        }
        next.boxes.push_back(box);
    }
}

void capture_storage(void* player) {
    static double previous{};
    static void* previous_dimension{};
    const double now=esp_time();
    auto* dimension=read<void*>(player,0x1C8);
    if(dimension==previous_dimension&&now-previous<0.2)return;
    previous=now;previous_dimension=dimension;
    Frame next;next.player=player;next.dimension=dimension;next.time=now;
    std::vector<chest_esp::Storage> containers;
    if(chest_esp::capture(player,image,containers)) {
        next.boxes.reserve(containers.size());
        constexpr COLORREF colors[]{RGB(240,175,80),RGB(255,215,65),RGB(205,115,245),RGB(240,140,85),RGB(255,80,80)};
        for(const auto& container:containers) {
            const auto* b=container.bounds;const auto kind=static_cast<unsigned>(container.kind);
            Box box{{b[0],b[1],b[2]},{b[3],b[4],b[5]},colors[kind]};box.storage_kind=kind;next.boxes.push_back(box);
        }
        if(!next.boxes.empty()&&!chest_snapshot_logged.exchange(true)) {
            char message[128]{};std::snprintf(message,sizeof(message),"ChestESP: native tick published %zu validated storage bounds.",next.boxes.size());Logger::instance().info(message);
        }
    }
    std::lock_guard lock(frame_mutex);storage_frame=std::move(next);
}

// Explicit release-build string layout: the mod's Debug CRT std::string has
// a different ABI and must never be passed into Minecraft.
struct GameString { union { char text[16]; const char* pointer; }; std::size_t size{}, capacity{15};
    GameString() : text{} {} explicit GameString(const char* s) : text{} {
        size=std::strlen(s); if(size<16) std::memcpy(text,s,size); else {pointer=s;capacity=size;}
    }
};
struct OptionalString { GameString value; bool present{}; Byte padding[7]{}; };
bool local_chat_supported_build(const integration::BedrockBuildInfo& build) noexcept {
    // The release-12645 display target is known. 26.50 changed native chat
    // internals and its display ABI/RVA has not been independently verified.
    return integration::is_release_12645(build);
}
bool local_chat(void* player, const char* text) {
    if(!player||!text||!local_chat_supported_build(integration::current_bedrock_build())) return false;
    auto* chat=read<void*>(read<void*>(player,0xD70),0x648);
    if(!chat||!integration::readable_game_memory(chat,sizeof(void*))) return false;
    using Display=void(__fastcall*)(void*,const GameString*,const OptionalString*,bool);
    const auto display=native<Display>(0x16DA850);
    if(!executable_game_code(reinterpret_cast<void*>(display))) return false;
    const GameString message(text);OptionalString source;
#if defined(_MSC_VER)
    __try { display(chat,&message,&source,false); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
#else
    display(chat,&message,&source,false);
#endif
    return true;
}
void startup_notice_tick(void* player) noexcept {
    if(!local_chat_supported_build(integration::current_bedrock_build())) {
        startup_notice.disarm();
        return;
    }
    if(startup_notice.try_send(player,[](void* current){return local_chat(current,startup_notice_text);}))
        Logger::instance().info("Startup chat notice displayed.");
}
bool copy_clipboard(const wchar_t* text) {
    const auto bytes=(std::wcslen(text)+1)*sizeof(wchar_t);
    auto memory=GlobalAlloc(GMEM_MOVEABLE,bytes); if(!memory) return false;
    auto* data=GlobalLock(memory); if(!data){GlobalFree(memory);return false;}
    std::memcpy(data,text,bytes); GlobalUnlock(memory);
    if(!OpenClipboard(nullptr)){GlobalFree(memory);return false;}
    const bool ok=EmptyClipboard() && SetClipboardData(CF_UNICODETEXT,memory);
    CloseClipboard(); if(!ok)GlobalFree(memory); return ok;
}
void death_position_tick_12650(void* player) noexcept {
    static wchar_t pending_clipboard[128]{};
    void* dimension{};
    const bool dimension_valid=camera_read(player,0x1C8,&dimension,sizeof(dimension))&&dimension;
    float health{};
    const bool health_valid=dimension_valid&&current_health(player,health);
    void* shape{};
    Vec3 position{};
    const bool position_valid=dimension_valid&&camera_read(player,0x220,&shape,sizeof(shape))&&shape&&
        camera_read(shape,0,&position,sizeof(position))&&valid(position);
    Vec3 death{};
    if(death_position_tracker_12650.update(player,dimension_valid?dimension:nullptr,
        health_valid,health,position_valid,position,death)&&on(GameplayFeature::deathposition)) {
        const int x=static_cast<int>(std::floor(death.x+0.3F));
        const int y=static_cast<int>(std::floor(death.y));
        const int z=static_cast<int>(std::floor(death.z+0.3F));
        char message[160]{};
        std::snprintf(message,sizeof(message),"Death position: %d, %d, %d",x,y,z);
        swprintf_s(pending_clipboard,L"%d %d %d",x,y,z);
        local_chat(player,message);
        Logger::instance().info(message);
    }
    if(pending_clipboard[0]&&copy_clipboard(pending_clipboard))pending_clipboard[0]=0;
}

void autotool(void* game_mode,const void* position) {
    if (!on(GameplayFeature::autotool) || !position) return;
    auto* player=read<Byte*>(game_mode,8); if(!local_player(player))return;
    auto* proxy=read<Byte*>(player,0x5B8);
    if(!integration::readable_game_memory(proxy,0xC0) || proxy[0xB0])return;
    auto* inventory=read<void*>(proxy,0xB8);
    auto get=method<void*(__fastcall*)(void*,int)>(inventory,0x38);
    auto size=method<int(__fastcall*)(void*)>(inventory,0xA0);
    if(!get || !size)return;
    const int selected=read<int>(proxy,0x10), count=(std::min)(36,size(inventory));
    if(selected<0 || selected>=9 || count<9)return;
    auto* dimension=read<void*>(player,0x1C8);
    auto get_region=method<void*(__fastcall*)(void*)>(dimension,0x60);
    if(!get_region)return;
    auto* region=get_region(dimension);
    auto get_block=method<void*(__fastcall*)(void*,const void*)>(region,0x10);
    if(!get_block)return;
    auto* block=get_block(region,position); if(!block)return;
    struct SpeedInput { void* actor_context; void* block; void* stack; void* cache; bool riding{}, grounded{true}, water{}, submerged{}, affinity{}; };
    auto score=[&](int slot) {
        auto* stack=get(inventory,slot);
        if(!read<void*>(stack,8) || !read<unsigned char>(stack,0x22))return 1.0F;
        SpeedInput input{player+8,block,stack,read<void*>(player,0xCB0)};
        return native<float(__fastcall*)(const SpeedInput*)>(0x380F370)(&input);
    };
    int best=selected; float best_speed=score(selected);
    for(int i=0;i<count;++i){const float speed=score(i);if(std::isfinite(speed)&&speed>best_speed+0.001F){best_speed=speed;best=i;}}
    if(best==selected)return;
    if(best<9){*reinterpret_cast<int*>(proxy+0x10)=best;return;}
    // Use game constructors/assignment and Container::setItem, preserving NBT,
    // enchantments, durability and stack network IDs. Native container listeners
    // produce the corresponding inventory transactions.
    alignas(16) Byte held[0x98]{}, tool[0x98]{};
    auto construct=native<void(__fastcall*)(void*)>(0x22D5460);
    auto assign=native<void*(__fastcall*)(void*,const void*)>(0x22D5C80);
    auto destroy=native<void(__fastcall*)(void*)>(0x1F8C30);
    auto set=method<void(__fastcall*)(void*,int,const void*)>(inventory,0x60);
    if(!set)return;
    construct(held);construct(tool);
    assign(held,get(inventory,selected));assign(tool,get(inventory,best));
    set(inventory,selected,tool);set(inventory,best,held);
    destroy(tool);destroy(held);
}
bool __fastcall start_hook(void* mode,const void* pos,unsigned char face,bool* destroyed) {
    autotool(mode,pos);return original_start(mode,pos,face,destroyed);
}
bool __fastcall creative_start_hook(void* mode,const void* pos,unsigned char face,bool* destroyed) {
    autotool(mode,pos);return original_creative_start(mode,pos,face,destroyed);
}
bool __fastcall continue_hook(void* mode,const void* pos,unsigned char face,const void* position,bool* destroyed) {
    autotool(mode,pos);return original_continue(mode,pos,face,position,destroyed);
}
bool __fastcall immune_hook(void* player,void* source) {
    if(on(GameplayFeature::phase) && read<int>(source,8)==4 && our_player(player))return true;
    return original_immune(player,source);
}

// The MobJumpSystem callback is shared by client prediction and server movement.
// Presence proxies are { sparse_pool*, entity_id }; they must not be replaced by
// a legacy Actor::onGround byte (this build has no such boolean).
bool component_present(const void* proxy) {
    auto* pool=read<void*>(proxy);const auto entity=read<std::uint32_t>(proxy,8);
    const auto index=entity&0x3FFFFU;
    const auto begin=read<std::uintptr_t>(pool,8),end=read<std::uintptr_t>(pool,16);
    if(!begin||end<begin||end-begin>8192||(end-begin)%8||index/2048>=(end-begin)/8)return false;
    const auto* page=read<Byte*>(reinterpret_cast<void*>(begin),(index/2048)*8);
    if(!integration::readable_game_memory(page,8192))return false;
    const auto packed=read<std::uint32_t>(page,(index%2048)*4);
    return (packed^ (entity&0xFFFC0000U))<0x3FFFFU;
}
void* airjump_actor(void* registry,std::uint32_t entity) {
    auto* entt=static_cast<Byte*>(registry)+0x30;
    auto* pool=pool_for(entt,0x85B93800);
    struct Proxy { void* pool; std::uint32_t entity; } proxy{pool,entity};
    if(!component_present(&proxy))return nullptr;
    const auto index=entity&0x3FFFFU;
    auto* page=read<void*>(read<void*>(pool,8),(index/2048)*8);
    const auto packed=read<std::uint32_t>(page,(index%2048)*4)&0x3FFFFU;
    const auto begin=read<std::uintptr_t>(pool,0x20),end=read<std::uintptr_t>(pool,0x28);
    if(!begin||end<begin||end-begin>65536||packed>=(end-begin)/4||read<std::uint32_t>(reinterpret_cast<void*>(begin),packed*4)!=entity)return nullptr;
    auto* actor=read<void*>(read<void*>(read<void*>(pool,0x50),(packed/128)*8),(packed%128)*8);
    return actor&&read<void*>(actor,0x10)==entt&&read<std::uint32_t>(actor,0x18)==entity?actor:nullptr;
}
using JumpSystem=void(__fastcall*)(void*,void*,void*,void*,void*,void*,void*,void*,void*,void*,void*,void*,void*,void*,void*,void*,void*,void*,void*);
JumpSystem original_jump{};
void dispatch_airjump(bool eligible,std::uint64_t now,void* id,void* jump_control,void* ground,void* player_component,
    void* squid,void* was_water,void* head_water,void* snow,void* lightweight,void* lava_drag,
    void* aabb,void* swim,void* effects,void* subboxes,void* actor_flags,void* jump_state,
    void* movement,void* registry,void* region) {
    bool extra=false;
    int previous_delay{};
    if(eligible&&airjump_requests.consume(reinterpret_cast<std::uintptr_t>(registry),read<std::uint32_t>(id),now)) {
            // Consume ground presses as well, so holding the normal takeoff key
            // cannot produce a second airborne jump. No world ground tag is changed.
            if(!component_present(ground)) {
                previous_delay=read<int>(jump_state,0x10);
                *reinterpret_cast<int*>(static_cast<Byte*>(jump_state)+0x10)=0;
                ground=player_component;extra=true;
            }
    }
    original_jump(id,jump_control,ground,player_component,squid,was_water,head_water,snow,lightweight,
        lava_drag,aabb,swim,effects,subboxes,actor_flags,jump_state,movement,registry,region);
    // If native rules refused the request (e.g. swimming), preserve its delay.
    if(extra&&read<int>(jump_state,0x10)==0)
        *reinterpret_cast<int*>(static_cast<Byte*>(jump_state)+0x10)=previous_delay;
}

// Called from the simulation callback, never from the overlay thread. Each
// local/server replica has independent smoothing and no retained Actor pointer.
void jetpack_movement(void* actor, void* movement, bool control) {
    struct Replica {
        std::uintptr_t actor{}, dimension{}, state{};
        std::uint32_t entity{};
        unsigned revision{};
        ULONGLONG time{};
        jetpack::Controller controller;
    };
    static thread_local std::array<Replica,4> replicas{};
    const auto identity=reinterpret_cast<std::uintptr_t>(actor);
    auto* slot=&replicas[0];
    for(auto& candidate:replicas) {
        if(candidate.actor==identity){slot=&candidate;break;}
        if(candidate.time<slot->time)slot=&candidate;
    }
    const auto now=GetTickCount64();
    const auto revision=jetpack_revision.load();
    const auto dimension=reinterpret_cast<std::uintptr_t>(read<void*>(actor,0x1C8));
    const auto entity=read<std::uint32_t>(actor,0x18);
    const auto state=reinterpret_cast<std::uintptr_t>(movement);
    if(slot->actor!=identity||slot->dimension!=dimension||slot->state!=state||slot->entity!=entity||
       slot->revision!=revision||now-slot->time>250)slot->controller.reset();
    slot->actor=identity;slot->dimension=dimension;slot->state=state;
    slot->entity=entity;slot->revision=revision;slot->time=now;
    auto* rotation=read<void*>(actor,0x228);
    if(!control||!on(GameplayFeature::jetpack)||!dimension||
       !integration::readable_game_memory(rotation,8)||!integration::readable_game_memory(movement,36)) {
        slot->controller.reset();return;
    }
    jetpack::Velocity next{};
    if(slot->controller.update(read<float>(rotation),read<float>(rotation,4),jetpack_speed.load(),
        read<jetpack::Velocity>(movement,24),next) && on(GameplayFeature::jetpack) && jetpack_revision.load()==revision)
        std::memcpy(static_cast<Byte*>(movement)+24,&next,sizeof(next));
}

void capture_esp_12650() {
    static double previous{};const auto now=esp_time();if(now-previous<0.05)return;previous=now;
    Frame next;next.player=integration::current_player();
    struct Publish { Frame& value; ~Publish(){value.time=esp_time();std::lock_guard lock(frame_mutex);frame=std::move(value);} } publish{next};
    const auto copy=[](std::uintptr_t at,void* out,std::size_t bytes){return camera_read(reinterpret_cast<void*>(at),0,out,bytes);};
    std::uintptr_t registry{};std::uint32_t id{};
    if(!camera_read(next.player,0x10,&registry,sizeof(registry))||
       !camera_read(next.player,0x18,&id,sizeof(id))||
       !camera_read(next.player,0x1C8,&next.dimension,sizeof(next.dimension))||!next.dimension)return;
    std::vector<esp_snapshot::Actor> actors;
    if(!esp_snapshot::actors(registry,reinterpret_cast<std::uintptr_t>(next.player),id,actors,copy))return;
    next.boxes.reserve(actors.size());
    for(const auto& entry:actors){auto* actor=reinterpret_cast<Byte*>(entry.pointer);
        if(actor==next.player)continue;
        void* shape{};void* dimension{};Box box{};
        struct ForeignString { char buffer[16];std::size_t size,capacity; } text{};
        if(!camera_read(actor,0x220,&shape,sizeof(shape))||!shape||
           !camera_read(shape,0,&box.lower,sizeof(Vec3))||!camera_read(shape,12,&box.upper,sizeof(Vec3))||
           !camera_read(actor,0x1C8,&dimension,sizeof(dimension))||
           !camera_read(actor,0x240,&text,sizeof(text))||!text.size||text.size>256||text.capacity<text.size||text.capacity>65536)continue;
        std::string name(text.size,'\0');
        if(text.capacity<16)std::memcpy(name.data(),text.buffer,text.size);
        else {void* data{};std::memcpy(&data,text.buffer,sizeof(data));if(!camera_read(data,0,name.data(),text.size))continue;}
        std::uintptr_t owner{};std::uint32_t actual{};void* dimension2{};
        if(!camera_read(actor,0x10,&owner,sizeof(owner))||!camera_read(actor,0x18,&actual,sizeof(actual))||
           !camera_read(actor,0x1C8,&dimension2,sizeof(dimension2))||owner!=registry||actual!=entry.entity||dimension2!=dimension||
           !esp_entity(actor,next.player,dimension,next.dimension,name,box))continue;
        box.player=name=="minecraft:player"||name.starts_with("minecraft:player.");
        box.color=box.player?RGB(255,50,50):name.starts_with("minecraft:")?RGB(55,135,255):RGB(60,235,90);
        void* state{};Vec3 positions[2]{};
        if(camera_read(actor,0x218,&state,sizeof(state))&&camera_read(state,0,positions,sizeof(positions))){
            const auto movement=minus(positions[0],positions[1]);
            if(valid(movement)&&dot(movement,movement)<16.0F)box.movement=movement;
        }
        next.boxes.push_back(box);
    }
    if(!next.boxes.empty()&&!esp_snapshot_logged.exchange(true)) {
        char message[128]{};std::snprintf(message,sizeof(message),"ESP: native tick published %zu validated entity bounds.",next.boxes.size());
        Logger::instance().info(message);
    }
}

void __fastcall esp_tick_12650(void* player) {
    original_tick(player);
    void* table{};if(!camera_read(player,0,&table,sizeof(table))||table!=image+0xE8E1BC0)return;
    integration::game_context_detail::player.store(player,std::memory_order_release);
    startup_notice_tick(player);
    refresh_world_seed_12650(player);
    death_position_tick_12650(player);
    // AutoLeave runs on the engine's verified LocalPlayer tick, after native
    // health updates, never on the overlay thread. Dimension changes rearm it.
    if(auto_leave_ready.load()) {
        static void* previous_player{};static void* previous_dimension{};
        void* dimension{};
        const bool valid=camera_read(player,0x1C8,&dimension,sizeof(dimension))&&dimension;
        const bool changed=previous_player!=player||previous_dimension!=dimension;
        previous_player=player;previous_dimension=valid?dimension:nullptr;
        if(valid) {
            auto_leave_tick(player,changed);
            if(auto_leave_gate.fired())return;
        }
        else auto_leave_gate.reset();
    }
    if(auto_bridge_ready.load())auto_bridge_tick(player);
    if(on(GameplayFeature::esp)) {
        try {capture_esp_12650();} catch(...) {
            std::lock_guard lock(frame_mutex);frame=Frame{};
        }
    }
    if(chest_ready.load()&&on(GameplayFeature::chest_esp)) {
        try {capture_storage(player);}catch(...) {std::lock_guard lock(frame_mutex);storage_frame=Frame{};}
    }
}

void __fastcall airjump_hook(void* id,void* jump_control,void* ground,void* player_component,
    void* squid,void* was_water,void* head_water,void* snow,void* lightweight,void* lava_drag,
    void* aabb,void* swim,void* effects,void* subboxes,void* actor_flags,void* jump_state,
    void* movement,void* registry,void* region) {
    bool eligible=false;
    void* actor{};
    if(on(GameplayFeature::airjump)&&airjump_ready.load()&&registry&&id&&component_present(player_component)) {
        actor=airjump_actor(registry,read<std::uint32_t>(id));
        eligible=actor&&our_player(actor)&&read<void*>(actor,0x218)==movement&&read<void*>(actor,0x1C8)&&
            integration::readable_game_memory(jump_state,0x14)&&controls_active()&&
            native<bool(__fastcall*)(void*)>(0x26EFD60)(actor);
    }
    dispatch_airjump(eligible&&on(GameplayFeature::airjump),GetTickCount64(),id,jump_control,ground,player_component,squid,was_water,
        head_water,snow,lightweight,lava_drag,aabb,swim,effects,subboxes,actor_flags,jump_state,movement,registry,region);
}

// Use the live level HitResult to validate the target. Attack delivery is a
// spaced local mouse press, never a re-entrant GameMode call from Player::tick.
bool verify_triggerbot() {
    struct Signature { std::uintptr_t rva; std::initializer_list<Byte> bytes; };
    const Signature signatures[]{
        {0xD72560,{0x48,0x8B,0x81,0xE8,0x01,0,0,0xC3}},
        {0x47E8B00,{0x48,0x83,0xEC,0x48,0x48,0x8D,0x51,0x38,0x48,0x8D,0x4C,0x24,0x28}},
        {0x26EFD60,{0x48,0x83,0xEC,0x28,0x80,0xB9,0x69,0x02,0,0,0}}
    };
    for (const auto& signature:signatures)
        if(std::memcmp(image+signature.rva,signature.bytes.begin(),signature.bytes.size())) return false;
    return true;
}
bool trigger_button_down{};
ULONGLONG trigger_button_time{};
bool send_trigger_mouse(DWORD mouse_flags) {
    INPUT input{};input.type=INPUT_MOUSE;input.mi.dwFlags=mouse_flags;
    return SendInput(1,&input,sizeof(input))==1;
}

// AutoBridge sends the same ordinary right-click a user would send. It never
// calls a game-mode placement method or writes inventory. A short pulse lets
// the normal game/server placement path accept or reject the action.
bool emit_bridge_place() {
    if(!controls_active()||integration::server_safety::remote_session())return false;
    if(!send_trigger_mouse(MOUSEEVENTF_RIGHTDOWN))return false;
    // Never intentionally leave the synthetic button held if Windows drops
    // the first release event. One bounded retry is safe and fail-closed.
    bool released=send_trigger_mouse(MOUSEEVENTF_RIGHTUP);
    if(!released)released=send_trigger_mouse(MOUSEEVENTF_RIGHTUP);
    if(!released)return false;
    static ULONGLONG last_log{};const auto now=GetTickCount64();
    if(now-last_log>=1000){last_log=now;Logger::instance().info("Auto Bridge dispatched guarded right-click input.");}
    return true;
}
void auto_bridge_tick(void* player) {
    static auto_bridge::Cadence cadence;
    static void* previous_player{};static void* previous_dimension{};
    const auto now=GetTickCount64();
    auto* dimension=read<void*>(player,0x1C8);
    if(previous_player!=player||previous_dimension!=dimension){cadence.reset();previous_player=player;previous_dimension=dimension;}
    const auto rotation=read<void*>(player,0x228);
    const auto pitch=read<float>(rotation,0);
    const auto supplies=read<void*>(player,0x5B8);
    int selected=-1;
    // Keep the relaxed 26.50 slot check that worked in testing, but do not let
    // a failed read silently become slot 0. ReadProcessMemory fails closed.
    const bool selected_slot=supplies&&camera_read(supplies,0x10,&selected,sizeof(selected))&&
        selected>=0&&selected<9;
    const auto controller=controller_controls();
    // The 26.50 player-alive target is not part of the verified profile yet;
    // controls are already disabled by the game when the local player is not
    // interactive. Keep this candidate fail-closed on pointer/dimension only
    // instead of calling an unverified native address.
    const bool alive=integration::is_release_12650(integration::current_bedrock_build())
        ? player&&dimension : player&&dimension&&native<bool(__fastcall*)(void*)>(0x26EFD60)(player);
    const auto input=auto_bridge::Input{
        on(GameplayFeature::auto_bridge),
        !integration::server_safety::remote_session(),
        controls_active(),
        alive,
        key('V')||controller.modifier,key('W')||controller.forward,key(VK_SPACE)||controller.jump,
        std::isfinite(pitch)&&pitch>=20.0F&&pitch<=89.5F,
        selected_slot};
    if(cadence.update(now,input).place)emit_bridge_place();
}
void release_trigger_mouse() {
    if(!trigger_button_down)return;
    send_trigger_mouse(MOUSEEVENTF_LEFTUP);trigger_button_down=false;trigger_button_time=0;
}
bool emit_local_attack() {
    if(trigger_button_down||!controls_active()||!integration::server_safety::local_world())return false;
    if(!send_trigger_mouse(MOUSEEVENTF_LEFTDOWN))return false;
    trigger_button_down=true;trigger_button_time=GetTickCount64();
    static ULONGLONG last_log{};const auto now=trigger_button_time;
    if(now-last_log>=1000){last_log=now;Logger::instance().info("Trigger Bot dispatched local attack input.");}
    return true;
}
using TriggerAction=bool(*)();
void triggerbot_tick(void* player, bool control, bool (*input_active)() = controls_active,
    TriggerAction attack_action = emit_local_attack) {
    static triggerbot::Cadence cadence(static_cast<unsigned>(GetTickCount64()) ^ GetCurrentProcessId());
    static unsigned revision{};
    const auto current_revision=trigger_revision.load();
    const auto now=GetTickCount64();
    if(trigger_button_down && (now<trigger_button_time || now-trigger_button_time>=15 ||
       !on(GameplayFeature::triggerbot) || !control || !input_active())) release_trigger_mouse();
    if(revision!=current_revision) { cadence.reset(); revision=current_revision; }
    const auto stop=[&] { cadence.reset(); };
    if(!on(GameplayFeature::triggerbot)||!trigger_ready.load()||!control||!local_player(player)||
       !integration::game_context_detail::picker_ready.load()) { stop(); return; }
    const auto observed=integration::game_context_detail::crosshair_time.load();
    if(!observed||now<observed||now-observed>100||
       integration::game_context_detail::crosshair_player.load()!=player) { stop(); return; }
    auto* level=read<void*>(player,0x1D8);
    if(read<void*>(read<void*>(level),0xA78)!=image+0xD72560) { stop(); return; }
    auto* hit=read<Byte*>(level,0x1E8);
    if(!integration::readable_game_memory(hit,0x88)||read<int>(hit,0x18)!=1||
       integration::game_context_detail::crosshair_hit.load()!=hit) { stop(); return; }
    auto* actor=native<void*(__fastcall*)(void*)>(0x47E8B00)(hit);
    auto* dimension=read<void*>(player,0x1C8);
    if(!actor||actor==player||!dimension||read<void*>(actor,0x1C8)!=dimension||
       read<void*>(actor,0x10)!=read<void*>(player,0x10)||!integration::readable_game_memory(actor,0x270)) { stop(); return; }
    const auto name=foreign_string(static_cast<Byte*>(actor)+0x240);
    const bool living=native<bool(__fastcall*)(void*)>(0x26EFD60)(actor);
    const auto type=triggerbot::classify(name,living);
    const auto options=trigger_targets.load();
    if(!esp_identifier(name)||!triggerbot::selected(type,options)) { stop(); return; }
    const auto position=read<Vec3>(hit,0x2C);
    if(!valid(position)) { stop(); return; }
    const triggerbot::TargetKey target{reinterpret_cast<std::uintptr_t>(actor),reinterpret_cast<std::uintptr_t>(dimension),read<std::uint32_t>(actor,0x18)};
    if(!cadence.update(esp_time(),target,true)) return;
    // Recheck foreground and menu settings immediately before the native attack.
    if(on(GameplayFeature::triggerbot)&&trigger_revision.load()==revision&&input_active())
        attack_action();
}

void tick_features(void* player,Vec3 before,bool alive_before) {
    static void* previous_player{};
    static bool sprint_owned{}, was_alive{};
    static Vec3 last_alive{};
    static wchar_t pending_clipboard[128]{};
    const bool changed=previous_player!=player;
    if(changed){++trigger_revision;airjump_requests.reset();previous_player=player;sprint_owned=false;was_alive=alive_before;}
    auto* state=read<Byte*>(player,0x218);auto* shape=read<Byte*>(player,0x220);
    if(!integration::readable_game_memory(state,36)||!integration::readable_game_memory(shape,32)){triggerbot_tick(player,false);return;}
    auto* position=reinterpret_cast<Vec3*>(state);auto* velocity=reinterpret_cast<Vec3*>(state+24);
    const bool alive=native<bool(__fastcall*)(void*)>(0x26EFD60)(player);
    local_state.store(state);
    local_unique_id.store(unique_id(player));
    integration::game_context_detail::player.store(player);
    startup_notice_tick(player);
    if(alive){last_alive=read<Vec3>(shape);was_alive=true;}
    else if(was_alive){
        was_alive=false;
        if(on(GameplayFeature::deathposition)){
            char message[160]{};std::snprintf(message,sizeof(message),"Death position: %d, %d, %d",
                static_cast<int>(std::floor(last_alive.x+0.3F)),static_cast<int>(std::floor(last_alive.y)),static_cast<int>(std::floor(last_alive.z+0.3F)));
            swprintf_s(pending_clipboard,L"%d %d %d",static_cast<int>(std::floor(last_alive.x+0.3F)),static_cast<int>(std::floor(last_alive.y)),static_cast<int>(std::floor(last_alive.z+0.3F)));
            local_chat(player,message);Logger::instance().info(message);
        }
    }
    if(pending_clipboard[0] && copy_clipboard(pending_clipboard))pending_clipboard[0]=0;
    auto_leave_tick(player,changed);
    const bool control=alive && controls_active();
    auto_bridge_tick(player);
    triggerbot_tick(player,control);
    const auto pressed=pending_keys.exchange(0);
    const auto down=[&](unsigned code){return key(code)||(pressed&key_bit(code))!=0;};
    if(!control)airjump_requests.reset();
    const float forward=control ? static_cast<float>(down('W'))-static_cast<float>(down('S')) : 0;
    const float side=control ? static_cast<float>(down('D'))-static_cast<float>(down('A')) : 0;
    const bool moving=forward!=0||side!=0;
    const bool sprint=on(GameplayFeature::autosprint)&&moving&&!key(VK_SHIFT);
    auto set_sprint=native<void(__fastcall*)(void*,bool)>(0x2F31080);
    if(sprint){set_sprint(player,true);sprint_owned=true;}
    else if(sprint_owned){set_sprint(player,false);sprint_owned=false;}
    if(on(GameplayFeature::phase)&&moving&&valid(before)&&valid(*position)){
        const float yaw=read<float>(read<void*>(player,0x228),4)*0.01745329252F;
        const float scale=(sprint?0.13F:0.10F)/std::sqrt(forward*forward+side*side);
        const float x=before.x+(-std::sin(yaw)*forward-std::cos(yaw)*side)*scale;
        const float z=before.z+(std::cos(yaw)*forward-std::sin(yaw)*side)*scale;
        const float dx=x-position->x,dz=z-position->z;
        // Retain the engine's vertical result and full-height collision box.
        // Only horizontal displacement bypasses the wall response.
        if(std::abs(dx)<2 && std::abs(dz)<2){
            position->x=x;position->z=z;
            auto* bounds=reinterpret_cast<Vec3*>(shape);bounds[0].x+=dx;bounds[1].x+=dx;bounds[0].z+=dz;bounds[1].z+=dz;
            velocity->x=0;velocity->z=0;
        }
    }
    if(on(GameplayFeature::esp))capture_esp(player);
    if(on(GameplayFeature::chest_esp))capture_storage(player);
}
void __fastcall local_tick_features(void* player) {
    integration::server_safety::observe_client_tick();
    const auto before=read<Vec3>(read<void*>(player,0x218));
    const bool alive_before=native<bool(__fastcall*)(void*)>(0x26EFD60)(player);
    integration::navigation_tick(player);
    original_tick(player);
    tick_features(player,before,alive_before);
}
bool jetpack_controls(void* player) {
    return controls_active() && native<bool(__fastcall*)(void*)>(0x26EFD60)(player);
}
// Player ticks run even when no jump or movement key is pressed. Thrust must
// follow native movement, not the input-filtered MobJumpSystem callback.
void dispatch_jetpack_tick(void* player, Tick tick, bool eligible,
    bool (*control)(void*) = jetpack_controls) {
    tick(player);
    if(eligible && on(GameplayFeature::jetpack))
        jetpack_movement(player,read<void*>(player,0x218),control(player));
}
void __fastcall tick_hook(void* player) {
    dispatch_jetpack_tick(player,local_tick_features,true);
}
void __fastcall server_tick_hook(void* player) {
    if(our_player(player)) integration::server_safety::observe_integrated_server_tick();
    dispatch_jetpack_tick(player,original_server_tick,our_player(player));
}
// The native closest-space system only adds horizontal ejection velocity.
// Suppress it for the local player while Phase is enabled; floor collision and
// every other entity still use the original engine paths.
void __fastcall eject_hook(void* a,void* b,void* c,void* d,void* e,void* state,void* g,bool h,void* i) {
    if(on(GameplayFeature::phase) && state && state==local_state.load())return;
    original_eject(a,b,c,d,e,state,g,h,i);
}
class GameplaySuspendedThreads final {
public:
    GameplaySuspendedThreads() noexcept {
        const DWORD process_id = GetCurrentProcessId();
        const DWORD current_thread_id = GetCurrentThreadId();
        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            return;
        }
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        if (Thread32First(snapshot, &entry) != FALSE) {
            do {
                if (entry.th32OwnerProcessID == process_id && entry.th32ThreadID != current_thread_id) {
                    HANDLE thread = OpenThread(
                        THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
                        FALSE, entry.th32ThreadID);
                    if (thread != nullptr) {
                        threads_.push_back(thread);
                    }
                }
            } while (Thread32Next(snapshot, &entry) != FALSE);
        }
        CloseHandle(snapshot);
        for (HANDLE thread : threads_) {
            if (SuspendThread(thread) == static_cast<DWORD>(-1)) {
                resume();
                return;
            }
            ++suspended_count_;
        }
        valid_ = true;
    }

    ~GameplaySuspendedThreads() noexcept {
        resume();
        for (HANDLE thread : threads_) {
            CloseHandle(thread);
        }
    }

    [[nodiscard]] bool valid() const noexcept { return valid_; }
    [[nodiscard]] bool outside(const void* address, std::size_t length) const noexcept {
        const auto begin = reinterpret_cast<std::uintptr_t>(address);
        const auto end = begin + length;
        for (std::size_t index = 0; index < suspended_count_; ++index) {
            CONTEXT context{};
            context.ContextFlags = CONTEXT_CONTROL;
            if (GetThreadContext(threads_[index], &context) == FALSE ||
                (context.Rip >= begin && context.Rip < end)) {
                return false;
            }
        }
        return true;
    }

private:
    void resume() noexcept {
        while (suspended_count_ != 0) {
            ResumeThread(threads_[--suspended_count_]);
        }
    }

    std::vector<HANDLE> threads_{};
    std::size_t suspended_count_{};
    bool valid_{};
};
bool install_airjump_hook() {
    auto* target=image+0xA0D8620;
    constexpr Byte signature[]{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x55,0x53,0x48,0x81,0xEC,0xC8,0,0,0};
    // Also validate the adapter's OnGround pool and native jump-request path.
    constexpr Byte ground_read[]{0xBA,0xA0,0x78,0x90,0xC2};
    constexpr Byte request[]{0xBA,0x76,0xC2,0xA8,0x4E};
    if(std::memcmp(target,signature,sizeof(signature))||
       std::memcmp(image+0xA102069,ground_read,sizeof(ground_read))||
       std::memcmp(image+0xA0D859F,request,sizeof(request)))return false;
    auto* trampoline=static_cast<Byte*>(VirtualAlloc(nullptr,64,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!trampoline)return false;
    const auto jump=[](Byte* to,void* dest){const Byte op[]{0xFF,0x25,0,0,0,0};std::memcpy(to,op,6);std::memcpy(to+6,&dest,8);};
    std::memcpy(trampoline,target,19);jump(trampoline+19,target+19);
    DWORD previous{};
    if(!VirtualProtect(trampoline,64,PAGE_EXECUTE_READ,&previous)){VirtualFree(trampoline,0,MEM_RELEASE);return false;}
    FlushInstructionCache(GetCurrentProcess(),trampoline,33);
    bool installed=false;
    {
        GameplaySuspendedThreads guard;
        DWORD protection{};
        if(guard.valid()&&guard.outside(target,19)&&VirtualProtect(target,19,PAGE_EXECUTE_READWRITE,&protection)) {
            original_jump=reinterpret_cast<JumpSystem>(trampoline);
            jump(target,reinterpret_cast<void*>(&airjump_hook));std::memset(target+14,0x90,5);
            FlushInstructionCache(GetCurrentProcess(),target,19);
            DWORD ignored{};VirtualProtect(target,19,protection,&ignored);installed=true;
        }
    }
    if(!installed)VirtualFree(trampoline,0,MEM_RELEASE);
    return installed;
}
bool install_ejection_hook() {
    auto* target=image+0x95BFD30;
    constexpr Byte signature[]{0x55,0x41,0x57,0x41,0x56,0x56,0x57,0x53,0x48,0x81,0xEC,0x58,0x01,0x00,0x00};
    if(std::memcmp(target,signature,sizeof(signature)))return false;
    auto* trampoline=static_cast<Byte*>(VirtualAlloc(nullptr,32,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    if(!trampoline)return false;
    const auto jump=[](Byte* to,void* dest){const Byte op[]{0xFF,0x25,0,0,0,0};std::memcpy(to,op,6);std::memcpy(to+6,&dest,8);};
    std::memcpy(trampoline,target,15);jump(trampoline+15,target+15);
    FlushInstructionCache(GetCurrentProcess(),trampoline,29);
    bool installed=false;
    {
        GameplaySuspendedThreads guard;
        DWORD protection{};
        if(guard.valid()&&guard.outside(target,15)&&VirtualProtect(target,15,PAGE_EXECUTE_READWRITE,&protection)) {
            original_eject=reinterpret_cast<Eject>(trampoline);
            jump(target,reinterpret_cast<void*>(&eject_hook));target[14]=0x90;
            FlushInstructionCache(GetCurrentProcess(),target,15);
            DWORD ignored{};VirtualProtect(target,15,protection,&ignored);installed=true;
        }
    }
    if(!installed)VirtualFree(trampoline,0,MEM_RELEASE);
    return installed;
}
bool swap_slot(std::uintptr_t rva,void* expected,void* replacement) {
    auto** slot=reinterpret_cast<void**>(image+rva);
    if(*slot!=expected)return false;
    DWORD old{};if(!VirtualProtect(slot,8,PAGE_READWRITE,&old))return false;
    const bool result=InterlockedCompareExchangePointer(slot,replacement,expected)==expected;
    DWORD ignored{};VirtualProtect(slot,8,old,&ignored);return result;
}
void initialize() {
    if(attempted.exchange(true))return;
    image=reinterpret_cast<Byte*>(GetModuleHandleW(nullptr));
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if(!image||dos->e_magic!=IMAGE_DOS_SIGNATURE)return;
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(image+dos->e_lfanew);
    if(nt->FileHeader.TimeDateStamp==0x6AA482FD&&nt->OptionalHeader.SizeOfImage==0x12C01000){
        esp_ready=false;
        constexpr Byte prologue[]{0x55,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x53,0x48,0x81,0xEC,0xD8,0x02,0,0};
        original_tick=native<Tick>(0x475EE60);
        if(std::memcmp(image+0x475EE60,prologue,sizeof(prologue))||
           !swap_slot(0xE8E1BC0+0xC0,reinterpret_cast<void*>(original_tick),reinterpret_cast<void*>(&esp_tick_12650))) {
            Logger::instance().info("ESP unavailable: exact 26.50 local tick slot/prologue mismatch.");return;
        }
        auto_bridge_ready=true;
        Logger::instance().info("Auto Bridge: 26.50 local tick connected; guarded input-assist candidate ready (V+W+Space, look down).");
        Logger::instance().info("ESP: 26.50 read-only packed snapshots connected to validated native local tick; waiting for local registry and camera validation. No native actor-list calls.");
        Logger::instance().info("Local chat display disabled on 26.50: native display target is unverified; startup and Death Position chat messages fail closed.");
        constexpr Byte leave_prologue[]{0x55,0x56,0x57,0x48,0x81,0xEC,0x00,0x01,0x00,0x00,0x48,0x8D,0xAC,0x24,0x80,0x00,0x00,0x00};
        constexpr Byte component_stride[]{0x4B,0x8D,0x04,0x80,0xC1,0xE0,0x04,0x48,0x01,0xC1};
        constexpr Byte instance_stride[]{0x4D,0x89,0xD0,0x4D,0x29,0xC8,0x4D,0x89,0xC1,0x49,0xC1,0xE1,0x05,0x4F,0x8D,0x04,0x41,0x4C,0x03,0x41,0x18};
        auto_leave_ready=read<void*>(image,0xE9731B0+14*8)==image+0x5DA3B00&&
            !std::memcmp(image+0x5DA3B00,leave_prologue,sizeof(leave_prologue))&&
            !std::memcmp(image+0x2539878,component_stride,sizeof(component_stride))&&
            !std::memcmp(image+0x31A129D,instance_stride,sizeof(instance_stride));
        Logger::instance().info(auto_leave_ready.load()?"AutoLeave: exact 26.50 save/disconnect method verified; LocalPlayer tick health monitor connected, waiting for validated health attributes.":"AutoLeave unavailable: exact 26.50 leave-game method mismatch.");
        chest_ready=chest_esp::profile_verified(image);
        Logger::instance().info(chest_ready.load()?"ChestESP: exact 26.50 block/chunk lookup prologues verified; native local tick and +0x50 storage bounds connected.":"ChestESP unavailable: exact 26.50 native storage profile mismatch.");return;
    }
    if(nt->FileHeader.TimeDateStamp!=0x6A8378BA||nt->OptionalHeader.SizeOfImage!=0x12888000)return;
    struct Slot { std::uintptr_t rva,target; void* hook; };
    const Slot slots[]{
        {0xE820EC0+0xC0,0x2F2C3E0,reinterpret_cast<void*>(&tick_hook)},
        {0xE820EC0+0x1F8,0x2FC8D80,reinterpret_cast<void*>(&immune_hook)},
        {0xE833530+0x1F8,0x2FC8D80,reinterpret_cast<void*>(&immune_hook)},
        {0xE81A090+8,0x2D2D640,reinterpret_cast<void*>(&start_hook)},
        {0xE81A130+8,0x2D33570,reinterpret_cast<void*>(&creative_start_hook)},
        {0xE81A090+0x18,0x2D2E310,reinterpret_cast<void*>(&continue_hook)},
        {0xE81A130+0x18,0x2D2E310,reinterpret_cast<void*>(&continue_hook)},
    };
    for(const auto& slot:slots)if(read<void*>(image,slot.rva)!=image+slot.target){Logger::instance().info("Gameplay hook signature mismatch; modules unavailable.");return;}
    original_tick=native<Tick>(0x2F2C3E0);original_immune=native<Immune>(0x2FC8D80);
    original_creative_start=native<StartMining>(0x2D33570);original_start=native<StartMining>(0x2D2D640);original_continue=native<ContinueMining>(0x2D2E310);
    std::size_t installed{};
    for(const auto& slot:slots){if(!swap_slot(slot.rva,image+slot.target,slot.hook))break;++installed;}
    if(installed!=std::size(slots)||!install_ejection_hook()){
        while(installed){const auto& slot=slots[--installed];swap_slot(slot.rva,slot.hook,image+slot.target);}return;
    }
    airjump_ready=install_airjump_hook();
    // Verified server Player::tick slot on 1.26.4501.0. Retain the original
    // callback for every server player; only our matching replica gets thrust.
    original_server_tick=native<Tick>(0x3344EC0);
    jetpack_ready=swap_slot(0xE833530+0xC0,reinterpret_cast<void*>(original_server_tick),
        reinterpret_cast<void*>(&server_tick_hook));
    trigger_ready=verify_triggerbot();
    auto_bridge_ready=true;
    Logger::instance().info(trigger_ready.load()?"Trigger Bot ready for local worlds; safe input delivery active.":
        "Trigger Bot unavailable: target-picker signature mismatch.");
    esp_ready=true;ready=true;Logger::instance().info("Native gameplay modules initialized for Minecraft 1.26.4501.0.");
}
}

void arm_startup_notice() noexcept { startup_notice.arm(); }
void disarm_startup_notice() noexcept { startup_notice.disarm(); }

std::string_view GameplayModule::name() const noexcept {
    constexpr std::string_view names[]{"ESP","Autotool","Phase","Airjump","deathposition","autosprint","ChestESP","triggerbot","Jetpack","Auto Leave","Auto Bridge"};
    return names[static_cast<unsigned>(feature_)];
}
ModuleCategory GameplayModule::category() const noexcept {
    switch(feature_){case GameplayFeature::triggerbot:case GameplayFeature::auto_leave:return ModuleCategory::combat;
    case GameplayFeature::esp:case GameplayFeature::chest_esp:return ModuleCategory::visual;
    case GameplayFeature::autotool:case GameplayFeature::deathposition:return ModuleCategory::player;
    default:return ModuleCategory::movement;}
}
bool GameplayModule::available() const noexcept {
    if(feature_==GameplayFeature::auto_leave&&integration::is_release_12650(integration::current_bedrock_build())) {
        float health{};return auto_leave_ready.load()&&current_health(integration::current_player(),health);
    }
    if(feature_==GameplayFeature::auto_bridge) return auto_bridge_ready.load();
    if(feature_==GameplayFeature::deathposition&&integration::is_release_12650(integration::current_bedrock_build())) {
        if(!image) return false;
        void* player=integration::current_player();
        float health{};
        void* shape{};
        Vec3 position{};
        return player&&current_health(player,health)&&
            camera_read(player,0x220,&shape,sizeof(shape))&&shape&&
            camera_read(shape,0,&position,sizeof(position))&&valid(position);
    }
    if(feature_==GameplayFeature::esp||(feature_==GameplayFeature::chest_esp&&integration::is_release_12650(integration::current_bedrock_build()))) {
        if(integration::is_release_12650(integration::current_bedrock_build())) {
            static std::mutex validation_mutex;std::lock_guard lock(validation_mutex);
            static double previous{};const double now=esp_time();
            if(now-previous>=0.5) {
                previous=now;Frame sample;sample.player=integration::current_player();
                std::uintptr_t registry{};std::uint32_t id{};std::vector<esp_snapshot::Actor> actors;
                const auto copy=[](std::uintptr_t at,void* out,std::size_t size){return camera_read(reinterpret_cast<void*>(at),0,out,size);};
                const bool valid=camera_read(sample.player,0x10,&registry,sizeof(registry))&&
                    camera_read(sample.player,0x18,&id,sizeof(id))&&
                    camera_read(sample.player,0x1C8,&sample.dimension,sizeof(sample.dimension))&&
                    esp_snapshot::actors(registry,reinterpret_cast<std::uintptr_t>(sample.player),id,actors,copy)&&refresh_camera(sample);
                if(valid&&!esp_ready.load())Logger::instance().info("ESP: local packed actor membership and camera projection validated; overlay ready.");
                esp_ready=valid;
            }
        }
        return esp_ready.load()&&(feature_!=GameplayFeature::chest_esp||chest_ready.load());
    }
    return ready.load() && (feature_!=GameplayFeature::jetpack || jetpack_ready.load()) &&
        (feature_!=GameplayFeature::airjump || airjump_ready.load()) && (feature_!=GameplayFeature::triggerbot ||
        (trigger_ready.load() && integration::game_context_detail::picker_ready.load()));
}
bool GameplayModule::allowed_on_remote_server() const noexcept { return feature_==GameplayFeature::auto_leave||feature_==GameplayFeature::esp||feature_==GameplayFeature::chest_esp; }
void GameplayModule::on_register(EventBus&) { initialize(); }
void GameplayModule::on_enable() { if(feature_==GameplayFeature::jetpack)++jetpack_revision; if(feature_==GameplayFeature::airjump)airjump_requests.reset(); if(feature_==GameplayFeature::triggerbot){++trigger_revision;Logger::instance().info("Trigger Bot enabled for local-world input delivery.");} if(feature_==GameplayFeature::auto_leave){++auto_leave_revision;Logger::instance().info("Auto Leave enabled.");} if(feature_==GameplayFeature::auto_bridge)Logger::instance().info("Auto Bridge enabled: keyboard V+W+Space or controller LB/RB/RT + left-stick-forward + A; look down."); flags[static_cast<unsigned>(feature_)]=true; }
void GameplayModule::on_disable() { flags[static_cast<unsigned>(feature_)]=false; if(feature_==GameplayFeature::jetpack)++jetpack_revision; if(feature_==GameplayFeature::airjump)airjump_requests.reset(); if(feature_==GameplayFeature::triggerbot){++trigger_revision;release_trigger_mouse();Logger::instance().info("Trigger Bot disabled.");} if(feature_==GameplayFeature::auto_leave)++auto_leave_revision; if(feature_==GameplayFeature::auto_bridge)Logger::instance().info("Auto Bridge disabled."); }
bool GameplayModule::has_value() const noexcept { return feature_==GameplayFeature::jetpack||feature_==GameplayFeature::auto_leave; }
std::string_view GameplayModule::value_label() const noexcept { return feature_==GameplayFeature::auto_leave?"Leave at":"Speed"; }
std::string_view GameplayModule::value_suffix() const noexcept { return feature_==GameplayFeature::auto_leave?" hearts":" blocks/s"; }
float GameplayModule::value() const noexcept { return feature_==GameplayFeature::auto_leave?auto_leave_hearts.load():feature_==GameplayFeature::jetpack?jetpack_speed.load():0.0F; }
float GameplayModule::minimum_value() const noexcept { return feature_==GameplayFeature::auto_leave?auto_leave::minimum_hearts:feature_==GameplayFeature::jetpack?jetpack::minimum_speed:0.0F; }
float GameplayModule::maximum_value() const noexcept { return feature_==GameplayFeature::auto_leave?auto_leave::maximum_hearts:feature_==GameplayFeature::jetpack?jetpack::maximum_speed:0.0F; }
void GameplayModule::set_value(float value) noexcept {
    if(feature_==GameplayFeature::auto_leave){auto_leave_hearts.store(auto_leave::clamp_hearts(value));return;}
    if(feature_==GameplayFeature::jetpack&&std::isfinite(value))jetpack_speed.store(std::clamp(value,minimum_value(),maximum_value()));
}
void GameplayModule::adjust_value(int direction) noexcept { if(direction)set_value(value()+(direction<0?(feature_==GameplayFeature::auto_leave?-0.5F:-1.0F):(feature_==GameplayFeature::auto_leave?0.5F:1.0F))); }
std::string_view GameplayModule::boolean_setting_name() const noexcept { return feature_==GameplayFeature::esp ? "Players only" : ""; }
bool GameplayModule::boolean_setting() const noexcept { return players_only.load(); }
void GameplayModule::set_boolean_setting(bool value) noexcept { if(feature_==GameplayFeature::esp)players_only=value; }
std::size_t GameplayModule::boolean_setting_count() const noexcept {
    if(feature_==GameplayFeature::triggerbot)return 2;
    return feature_==GameplayFeature::chest_esp?chest_esp::kind_count:Module::boolean_setting_count();
}
std::string_view GameplayModule::boolean_setting_name(std::size_t index) const noexcept {
    if(feature_==GameplayFeature::triggerbot)return index==0?"Mobs":index==1?"Players":"";
    return feature_==GameplayFeature::chest_esp?(index<chest_esp::kind_count?chest_esp::labels[index]:std::string_view{}):Module::boolean_setting_name(index);
}
bool GameplayModule::boolean_setting(std::size_t index) const noexcept {
    if(feature_==GameplayFeature::triggerbot)return index<2 && (trigger_targets.load()&(1U<<index))!=0;
    return feature_==GameplayFeature::chest_esp?(index<chest_esp::kind_count&&(storage_types.load()&(1U<<index))!=0):Module::boolean_setting(index);
}
void GameplayModule::set_boolean_setting(std::size_t index,bool value) noexcept {
    if(feature_==GameplayFeature::triggerbot){
        if(index<2){if(value)trigger_targets.fetch_or(1U<<index);else trigger_targets.fetch_and(~(1U<<index));++trigger_revision;}
        return;
    }
    if(feature_!=GameplayFeature::chest_esp){Module::set_boolean_setting(index,value);return;}
    if(index>=chest_esp::kind_count)return;
    if(value)storage_types.fetch_or(1U<<index);else storage_types.fetch_and(~(1U<<index));
}
void GameplayModule::on_key_down(unsigned code) noexcept {
    if(feature_==GameplayFeature::airjump&&code==VK_SPACE)airjump_requests.press(GetTickCount64());
    if(feature_==GameplayFeature::phase||feature_==GameplayFeature::airjump||feature_==GameplayFeature::autosprint)
        pending_keys.fetch_or(key_bit(code));
}
void GameplayModule::on_key_up(unsigned code) noexcept {
    if(feature_==GameplayFeature::airjump&&code==VK_SPACE)airjump_requests.release();
}
void GameplayModule::draw_overlay(void* target,int width,int height) noexcept {
    const bool storage=feature_==GameplayFeature::chest_esp;
    if((feature_!=GameplayFeature::esp&&!storage)||!enabled()||!available())return;
    Frame copy;{std::lock_guard lock(frame_mutex);copy=storage?storage_frame:frame;}
    auto& camera_cache=storage?last_storage_camera:last_camera;
    const double age=esp_time()-copy.time;
    if(age<0||age>(storage?0.6:0.15)){camera_cache.present=false;return;}
    if(!cached_camera(copy,camera_cache,esp_time()))return;
    HDC dc=static_cast<HDC>(target);
    const auto camera=[&](Vec3 point){return camera_space(copy,point);};
    constexpr int edges[][2]{{0,1},{1,3},{3,2},{2,0},{4,5},{5,7},{7,6},{6,4},{0,4},{1,5},{2,6},{3,7}};
    for(const auto& box:copy.boxes){
        if(storage){if(!(storage_types.load()&(1U<<box.storage_kind)))continue;}
        else if(players_only.load()&&!box.player)continue;
        const auto offset=interpolated_offset(box,age);
        Vec3 corners[8];for(unsigned i=0;i<8;++i)corners[i]=camera({
            (i&1?box.upper.x:box.lower.x)+offset.x,
            (i&2?box.upper.y:box.lower.y)+offset.y,
            (i&4?box.upper.z:box.lower.z)+offset.z});
        auto pen=CreatePen(PS_SOLID,1,box.color);auto old=SelectObject(dc,pen);
        for(const auto& edge:edges){
            auto a=corners[edge[0]],b=corners[edge[1]];constexpr float near_plane=0.05F;
            if(a.z<near_plane&&b.z<near_plane)continue;
            if(a.z<near_plane||b.z<near_plane){const float t=(near_plane-a.z)/(b.z-a.z);const Vec3 cut{a.x+t*(b.x-a.x),a.y+t*(b.y-a.y),near_plane};if(a.z<near_plane)a=cut;else b=cut;}
            const auto screen=[&](Vec3 v){return POINT{static_cast<LONG>(std::clamp(width*0.5F*(1+v.x*copy.scale_x/v.z),-100000.0F,100000.0F)),static_cast<LONG>(std::clamp(height*0.5F*(1-v.y*copy.scale_y/v.z),-100000.0F,100000.0F))};};
            auto p=screen(a),q=screen(b);MoveToEx(dc,p.x,p.y,nullptr);LineTo(dc,q.x,q.y);
            if(!storage&&!esp_draw_logged.exchange(true))Logger::instance().info("ESP: projected entity edges submitted to the live overlay.");
            if(storage&&!chest_draw_logged.exchange(true))Logger::instance().info("ChestESP: projected storage edges submitted to the live overlay.");
        }
        SelectObject(dc,old);DeleteObject(pen);
    }
}
}


#include "ReachModule.hpp"

#include "../framework/Logger.hpp"
#include "../integration/GameContext.hpp"

#include <TlHelp32.h>
#include <intrin.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace utility::modules {
namespace {

struct ReachProfile final {
    std::uint32_t timestamp;
    std::uint32_t image_size;
    std::uintptr_t pick_range_rva;
    std::uintptr_t max_pick_range_rva;
    std::array<std::uintptr_t, 2> pick_slot_rvas;
    bool full_entity_support;
    std::uintptr_t final_range_rva;
    std::uintptr_t picker_return_rva;
};

constexpr ReachProfile release_12645{
    0x6A8378BA, 0x12888000, 0x2D329E0, 0x2D32A80,
    {0xE81A0E0, 0xE81A180}, true, 0x1B4B6F0, 0x4F63D3};

// Exact 26.50 native picker, all three Survival reads and final HitResult
// range validation were inspected together in the mapped executable.
constexpr ReachProfile release_12650{
    0x6AA482FD, 0x12C01000, 0x259FC40, 0x259FCE0,
    {0xE8275B0, 0xE827650}, true, 0xEC3D80, 0x4BD1A3};
constexpr std::size_t max_patch_size = 14;
constexpr float minimum_distance = 3.0F;
// Keep the familiar 7-block default, but allow an extended opt-in range.
constexpr float maximum_distance = 10.0F;
constexpr float distance_step = 0.5F;

constexpr std::array<std::byte, 16> expected_pick_prologue{
    std::byte{0x56}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x28}, std::byte{0x8B}, std::byte{0x02},
    std::byte{0x83}, std::byte{0xF8}, std::byte{0x01}, std::byte{0x74},
    std::byte{0x0F}, std::byte{0x83}, std::byte{0xF8}, std::byte{0x03},
};

constexpr std::array<std::byte, max_patch_size> expected_max_prologue{
    std::byte{0x56}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x28}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0x79}, std::byte{0x08}, std::byte{0x48}, std::byte{0x8D},
    std::byte{0x4F}, std::byte{0x08},
};

std::atomic<ReachModule*> active_module{};
std::atomic_uint32_t active_calls{};
thread_local float selected_block_range = 5.0F;
thread_local float selected_vanilla_range = 5.0F;
struct PickRanges { float ray, block; };
PickRanges pick_ranges(float vanilla, bool entity_on, float entity, bool block_on, float block) noexcept {
    const float block_limit=block_on?block:vanilla;
    return {(std::max)((std::max)(vanilla,block_limit),entity_on?entity:0.0F),block_limit};
}
float final_range(int type, float native_range, float block, bool entity_on, float entity,float vanilla=std::numeric_limits<float>::max()) noexcept {
    if(type==0)return block;
    if(type==1&&entity_on)return entity;
    if(type==1)return (std::min)(native_range,vanilla);
    return native_range;
}


class ActiveCall final {
public:
    ActiveCall() noexcept { active_calls.fetch_add(1, std::memory_order_acq_rel); }
    ~ActiveCall() noexcept { active_calls.fetch_sub(1, std::memory_order_acq_rel); }
};

class SuspendedThreads final {
public:
    SuspendedThreads() noexcept {
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

    ~SuspendedThreads() noexcept {
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


// The live crosshair picker has its own Survival entity cap, independent of
// GameMode::getPickRange. Redirect only these three reads, not the shared 3.0
// constant (which is used by unrelated game systems).
struct EntityRangeRead {
    std::uintptr_t rva;
    std::size_t size;
    std::array<unsigned char, 8> bytes;
};
constexpr std::array<EntityRangeRead, 3> entity_range_reads{{
    {0x4F5E8C, 7, {0x0F,0x2E,0x3D,0xA9,0xF1,0x10,0x0E}},
    {0x4F5E95, 8, {0xF3,0x0F,0x10,0x3D,0x9F,0xF1,0x10,0x0E}},
    {0x4F5FDC, 7, {0x0F,0x2E,0x1D,0x59,0xF0,0x10,0x0E}},
}};
constexpr std::array<EntityRangeRead,3> entity_range_reads_12650{{
    {0x4BCC5C,7,{0x0F,0x2E,0x3D,0x49,0x56,0x14,0x0E}},
    {0x4BCC65,8,{0xF3,0x0F,0x10,0x3D,0x3F,0x56,0x14,0x0E}},
    {0x4BCDAC,7,{0x0F,0x2E,0x1D,0xF9,0x54,0x14,0x0E}}
}};

class EntityRangeStorage final {
public:
    bool install(std::byte* image,bool native_12650=false) noexcept {
        if (installed_) return image == base_;
        sites_=native_12650?entity_range_reads_12650:entity_range_reads;
        for (const auto& site : sites_) {
            if (std::memcmp(image + site.rva, site.bytes.data(), site.size)) return false;
        }
        if (value_ == nullptr) {
            const auto origin = reinterpret_cast<std::uintptr_t>(image + sites_[0].rva) & ~std::uintptr_t{0xFFFF};
            // A private aligned float within RIP-relative addressing distance.
            for (std::uintptr_t delta = 0x10000; delta < 0x70000000 && !value_; delta += 0x10000) {
                for (const auto candidate : {origin + delta, origin > delta ? origin - delta : std::uintptr_t{0}}) {
                    if (!candidate) continue;
                    MEMORY_BASIC_INFORMATION region{};
                    if (VirtualQuery(reinterpret_cast<void*>(candidate), &region, sizeof(region)) && region.State == MEM_FREE) {
                        value_ = static_cast<LONG*>(VirtualAlloc(reinterpret_cast<void*>(candidate), 4096,
                            MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
                        if (value_) break;
                    }
                }
            }
        }
        if (!value_) return false;
        set(3.0F);
        base_ = image;
        for (std::size_t i = 0; i < sites_.size(); ++i) {
            const auto& site = sites_[i];
            const auto displacement = reinterpret_cast<std::intptr_t>(value_) -
                reinterpret_cast<std::intptr_t>(image + site.rva + site.size);
            if (displacement < INT32_MIN || displacement > INT32_MAX) return false;
            redirected_[i] = site.bytes;
            const auto offset = static_cast<std::int32_t>(displacement);
            std::memcpy(redirected_[i].data() + site.size - 4, &offset, 4);
        }
        return installed_ = rewrite(true);
    }

    void set(float distance) noexcept {
        if (!value_) return;
        LONG bits{};
        std::memcpy(&bits, &distance, sizeof(bits));
        InterlockedExchange(value_, bits);
    }

    bool uninstall() noexcept {
        set(3.0F);
        if (!installed_) return true;
        if (!rewrite(false)) return false;
        installed_ = false;
        // Retain the tiny allocation until process exit. A picker already past
        // the patched instruction must never observe released backing storage.
        return true;
    }

private:
    bool rewrite(bool redirect) noexcept {
        SuspendedThreads suspended;
        if (!suspended.valid()) return false;
        for (std::size_t i = 0; i < sites_.size(); ++i) {
            const auto& site = sites_[i];
            if (!suspended.outside(base_ + site.rva, site.size) ||
                std::memcmp(base_ + site.rva, (redirect ? site.bytes : redirected_[i]).data(), site.size)) return false;
        }
        // All three complete instructions are in the same executable page.
        auto* page = base_ + (sites_[0].rva & ~std::uintptr_t{4095});
        DWORD protection{};
        if (!VirtualProtect(page, 4096, PAGE_EXECUTE_READWRITE, &protection)) return false;
        for (std::size_t i = 0; i < sites_.size(); ++i) {
            const auto& site = sites_[i];
            std::memcpy(base_ + site.rva, (redirect ? redirected_[i] : site.bytes).data(), site.size);
        }
        FlushInstructionCache(GetCurrentProcess(), page, 4096);
        DWORD ignored{};
        VirtualProtect(page, 4096, protection, &ignored);
        return true;
    }
    std::byte* base_{};
    LONG* value_{};
    bool installed_{};
    std::array<EntityRangeRead,3> sites_=entity_range_reads;
    std::array<std::array<unsigned char, 8>, 3> redirected_{};
};

EntityRangeStorage& entity_range_storage() {
    static auto* storage = new EntityRangeStorage;
    return *storage;
}

void write_absolute_jump(std::byte* destination, const void* target) noexcept {
    destination[0] = std::byte{0xFF};
    destination[1] = std::byte{0x25};
    std::memset(destination + 2, 0, 4);
    const auto value = reinterpret_cast<std::uintptr_t>(target);
    std::memcpy(destination + 6, &value, sizeof(value));
}

[[nodiscard]] const ReachProfile* reach_profile(HMODULE image) noexcept {
    if (image == nullptr) {
        return nullptr;
    }
    wchar_t path[MAX_PATH]{};
    if (GetModuleFileNameW(image, path, MAX_PATH) == 0) {
        return nullptr;
    }
    const wchar_t* filename = path;
    for (const wchar_t* cursor = path; *cursor != L'\0'; ++cursor) {
        if (*cursor == L'\\' || *cursor == L'/') {
            filename = cursor + 1;
        }
    }
    if (_wcsicmp(filename, L"Minecraft.Windows.exe") != 0) {
        return nullptr;
    }
    const auto* base = reinterpret_cast<const std::byte*>(image);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return nullptr;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;
    for (const auto* profile : {&release_12645, &release_12650}) {
        if (nt->FileHeader.TimeDateStamp == profile->timestamp &&
            nt->OptionalHeader.SizeOfImage == profile->image_size) return profile;
    }
    return nullptr;
}

bool exchange_slot(void** slot, void* value) noexcept {
    DWORD old_protection{};
    if (VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old_protection) == FALSE) {
        return false;
    }
    InterlockedExchangePointer(slot, value);
    DWORD ignored{};
    VirtualProtect(slot, sizeof(void*), old_protection, &ignored);
    return true;
}

} // namespace

ReachModule::~ReachModule() {
    active_.store(false, std::memory_order_release);
    uninstall_hooks();
}

std::string_view ReachModule::name() const noexcept { return "EntityReach"; }
ModuleCategory ReachModule::category() const noexcept { return ModuleCategory::combat; }
bool ReachModule::available() const noexcept { return hooks_installed_; }
bool ReachModule::has_value() const noexcept { return true; }
float ReachModule::value() const noexcept { return distance_.load(std::memory_order_acquire); }
float ReachModule::minimum_value() const noexcept { return minimum_distance; }
float ReachModule::maximum_value() const noexcept { return maximum_distance; }

void ReachModule::adjust_value(int direction) noexcept {
    if (direction == 0) {
        return;
    }
    const float old_value = distance_.load(std::memory_order_acquire);
    const float next = std::clamp(
        old_value + (direction < 0 ? -distance_step : distance_step),
        minimum_distance, maximum_distance);
    distance_.store(next, std::memory_order_release);
    if (active_.load(std::memory_order_acquire)) entity_range_storage().set(next);
}

void ReachModule::set_value(float distance) noexcept {
    if (!std::isfinite(distance)) return;
    distance_.store(std::clamp(std::round(distance / distance_step) * distance_step,
        minimum_distance, maximum_distance), std::memory_order_release);
    if (active_.load(std::memory_order_acquire)) entity_range_storage().set(value());
}

void ReachModule::on_register(EventBus&) {
    if (install_hooks()) {
        Logger::instance().info(release_12650_
            ? "Reach hooks installed for Minecraft 1.26.5101.0; verified ray, Survival cap and final crosshair distance gates connected. Local interaction maximum includes both independent sliders; remote servers may reject extended interactions."
            : "Reach hooks installed for Minecraft 1.26.4501.0; block selection, Survival entity selection, and singleplayer validation use the live slider.");
    } else {
        Logger::instance().info(
            "Reach unavailable: exact supported range signatures were not present; no memory was changed.");
    }
}

void ReachModule::on_enable() {
    ray_reported_=false;final_reported_=false;
    entity_range_storage().set(value());
    active_.store(true, std::memory_order_release);
    Logger::instance().info("Reach enabled; distance is controlled by the menu slider.");
}

void ReachModule::on_disable() {
    active_.store(false, std::memory_order_release);
    entity_range_storage().set(3.0F);
    Logger::instance().info("Reach disabled; vanilla range behavior restored.");
}

float __fastcall ReachModule::pick_range_hook(
    void* game_mode, const int* input_mode, bool include_liquids) noexcept {
    const ActiveCall call;
    integration::observe_game_mode(game_mode);
    ReachModule* module = active_module.load(std::memory_order_acquire);
    if (module == nullptr || module->original_pick_range_ == nullptr) {
        return 3.0F;
    }
    const float vanilla = module->original_pick_range_(game_mode, input_mode, include_liquids);
    const auto ranges=pick_ranges(vanilla,module->active_.load(),module->value(),module->block_active_.load(),module->block_value());
    selected_block_range=ranges.block;
    selected_vanilla_range=vanilla;
    if((module->active_.load()||module->block_active_.load())&&!module->ray_reported_.exchange(true)) {
        char message[180]{};std::snprintf(message,sizeof(message),"Reach: native pick callback received slider ranges; vanilla=%.1f, ray=%.1f, blocks=%.1f.",vanilla,ranges.ray,ranges.block);
        Logger::instance().info(message);
    }
    return ranges.ray;
}

float __fastcall ReachModule::max_pick_range_hook(void* game_mode) noexcept {
    const ActiveCall call;
    integration::observe_game_mode(game_mode);
    ReachModule* module = active_module.load(std::memory_order_acquire);
    if (module == nullptr || module->original_max_pick_range_ == nullptr) {
        return 6.7F;
    }
    const float vanilla = module->original_max_pick_range_(game_mode);
    return pick_ranges(vanilla,module->active_.load(),module->value(),module->block_active_.load(),module->block_value()).ray;
}

void ReachModule::set_block_value(float distance) noexcept {
    if (std::isfinite(distance)) block_distance_.store(std::clamp(std::round(distance * 2.0F) / 2.0F, 3.0F, maximum_distance));
}

void __fastcall ReachModule::final_range_hook(void* hit, void* player, float range, bool adjust) noexcept {
    const ActiveCall call;
    auto* module = active_module.load();
    if (!module || !module->original_final_range_) return;
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    // Only the crosshair picker receives the combined ray. Other callers retain
    // their own native ranges. A longer entity ray must not extend block reach.
    const bool picker=reinterpret_cast<std::uintptr_t>(_ReturnAddress())==base+module->picker_return_rva_;
    if (picker && hit && (module->active_.load()||module->block_active_.load())) {
        const int type = *reinterpret_cast<const int*>(static_cast<const std::byte*>(hit) + 0x18);
        range=final_range(type,range,selected_block_range,module->active_.load(),module->value(),selected_vanilla_range);
        if((module->active_.load()||module->block_active_.load())&&!module->final_reported_.exchange(true)) {
            char message[180]{};std::snprintf(message,sizeof(message),"Reach: native final crosshair gate reached; hit type=%d, selected limit=%.1f.",type,range);
            Logger::instance().info(message);
        }
    }
    module->original_final_range_(hit, player, range, adjust);
    if (picker && !module->release_12650_)
        integration::observe_crosshair(player, hit);
}

bool ReachModule::install_hooks() noexcept {
    if (hooks_installed_) {
        return true;
    }
    HMODULE image = GetModuleHandleW(nullptr);
    const auto* profile = reach_profile(image);
    if (profile == nullptr) {
        return false;
    }
    auto* base = reinterpret_cast<std::byte*>(image);
    auto* pick_target = base + profile->pick_range_rva;
    auto* max_target = base + profile->max_pick_range_rva;
    if(profile==&release_12650) {
        // Exact native crosshair caller: hit in RCX, player in RDX, range in
        // XMM2, adjustment in R9. The shared float constant is never modified.
        const unsigned char caller[]{0x48,0x89,0xC1,0x4C,0x89,0xFA,0x0F,0x28,0xD6,0x41,0x89,0xD9,0xE8,0xDD,0x6B,0xA0,0x00};
        const unsigned char picker_entry[]{0x55,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x53,0x48,0x81,0xEC,0x58,0x06,0x00,0x00};
        if(std::memcmp(base+0x4BD192,caller,sizeof(caller))||std::memcmp(base+0x4BA840,picker_entry,sizeof(picker_entry)))return false;
        for(const auto& site:entity_range_reads_12650)if(std::memcmp(base+site.rva,site.bytes.data(),site.size))return false;
    }
    picker_return_rva_=profile->picker_return_rva;
    if (std::memcmp(pick_target, expected_pick_prologue.data(), expected_pick_prologue.size()) != 0 ||
        std::memcmp(max_target, expected_max_prologue.data(), expected_max_prologue.size()) != 0) {
        return false;
    }
    for (std::size_t index = 0; index < profile->pick_slot_rvas.size(); ++index) {
        pick_slots_[index] = reinterpret_cast<void**>(base + profile->pick_slot_rvas[index]);
        if (*pick_slots_[index] != pick_target) {
            return false;
        }
    }

    auto* trampoline = static_cast<std::byte*>(VirtualAlloc(
        nullptr, max_patch_size + 14, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (trampoline == nullptr) {
        return false;
    }
    std::memcpy(trampoline, max_target, max_patch_size);
    write_absolute_jump(trampoline + max_patch_size, max_target + max_patch_size);
    FlushInstructionCache(GetCurrentProcess(), trampoline, max_patch_size + 14);

    if (profile->full_entity_support) {
        const unsigned char final_expected[]{0x41,0x56,0x56,0x57,0x53,0x48,0x81,0xEC,0x88,0,0,0,0x44,0x0F,0x29,0x5C,0x24,0x70};
        auto* final_target = base + profile->final_range_rva;
        if (std::memcmp(final_target, final_expected, sizeof(final_expected))) {
            VirtualFree(trampoline, 0, MEM_RELEASE); return false;
        }
        auto* final_trampoline = static_cast<std::byte*>(VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (!final_trampoline) { VirtualFree(trampoline, 0, MEM_RELEASE); return false; }
        std::memcpy(final_original_, final_target, 18);
        std::memcpy(final_trampoline, final_target, 18);
        write_absolute_jump(final_trampoline + 18, final_target + 18);
        FlushInstructionCache(GetCurrentProcess(), final_trampoline, 32);
        original_final_range_ = reinterpret_cast<FinalRangeFunction>(final_trampoline);
        final_target_ = final_target;
    }

    original_pick_range_ = reinterpret_cast<PickRangeFunction>(pick_target);
    original_max_pick_range_ = reinterpret_cast<MaxPickRangeFunction>(trampoline);
    active_module.store(this, std::memory_order_release);

    if (!exchange_slot(pick_slots_[0], reinterpret_cast<void*>(&pick_range_hook)) ||
        !exchange_slot(pick_slots_[1], reinterpret_cast<void*>(&pick_range_hook))) {
        (void)exchange_slot(pick_slots_[0], pick_target);
        (void)exchange_slot(pick_slots_[1], pick_target);
        active_module.store(nullptr, std::memory_order_release);
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return false;
    }

    {
    SuspendedThreads suspended;
    if (!suspended.valid() || !suspended.outside(max_target, max_patch_size)) {
        (void)exchange_slot(pick_slots_[0], pick_target);
        (void)exchange_slot(pick_slots_[1], pick_target);
        active_module.store(nullptr, std::memory_order_release);
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return false;
    }
    DWORD old_protection{};
    if (VirtualProtect(max_target, max_patch_size, PAGE_EXECUTE_READWRITE, &old_protection) == FALSE) {
        (void)exchange_slot(pick_slots_[0], pick_target);
        (void)exchange_slot(pick_slots_[1], pick_target);
        active_module.store(nullptr, std::memory_order_release);
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return false;
    }
    std::memcpy(max_original_, max_target, max_patch_size);
    write_absolute_jump(max_target, reinterpret_cast<void*>(&max_pick_range_hook));
    FlushInstructionCache(GetCurrentProcess(), max_target, max_patch_size);
    DWORD ignored{};
    VirtualProtect(max_target, max_patch_size, old_protection, &ignored);
    }

    max_target_ = max_target;
    max_trampoline_ = trampoline;
    full_entity_support_ = profile->full_entity_support;
    release_12650_ = profile == &release_12650;
    hooks_installed_ = true;
    if (full_entity_support_ && !entity_range_storage().install(base,release_12650_)) {
        uninstall_hooks();
        return false;
    }
    if (!full_entity_support_) return true;
    bool final_installed = false;
    {
        SuspendedThreads suspended;
        DWORD protection{};
        if (suspended.valid() && suspended.outside(final_target_, 18) &&
            VirtualProtect(final_target_, 18, PAGE_EXECUTE_READWRITE, &protection)) {
        write_absolute_jump(static_cast<std::byte*>(final_target_), reinterpret_cast<void*>(&final_range_hook));
        std::memset(static_cast<std::byte*>(final_target_) + 14, 0x90, 4);
        FlushInstructionCache(GetCurrentProcess(), final_target_, 18);
        DWORD ignored{}; VirtualProtect(final_target_, 18, protection, &ignored);
        final_installed = true;
        }
    }
    if (!final_installed) { uninstall_hooks(); return false; }
    // Keep old Trigger Bot/player-layout integration gated on 26.50.
    integration::game_context_detail::picker_ready = !release_12650_;
    return true;
}

void ReachModule::uninstall_hooks() noexcept {
    integration::game_context_detail::picker_ready=false;
    integration::game_context_detail::crosshair_time=0;
    if (!hooks_installed_) {
        return;
    }
    active_.store(false, std::memory_order_release);
    if (full_entity_support_ && !entity_range_storage().uninstall()) return;
    auto* pick_target = reinterpret_cast<void*>(original_pick_range_);
    (void)exchange_slot(pick_slots_[0], pick_target);
    (void)exchange_slot(pick_slots_[1], pick_target);

    {
        SuspendedThreads suspended;
        if (!suspended.valid() || !suspended.outside(max_target_, max_patch_size) ||
            !suspended.outside(max_trampoline_, max_patch_size + 14)) {
            return;
        }
        DWORD old_protection{};
        if (VirtualProtect(max_target_, max_patch_size, PAGE_EXECUTE_READWRITE, &old_protection) == FALSE) {
            return;
        }
        std::memcpy(max_target_, max_original_, max_patch_size);
        FlushInstructionCache(GetCurrentProcess(), max_target_, max_patch_size);
        DWORD ignored{};
        VirtualProtect(max_target_, max_patch_size, old_protection, &ignored);
    }

    if (final_target_) {
        SuspendedThreads suspended;
        DWORD protection{};
        if (!suspended.valid() || !suspended.outside(final_target_, 18) ||
            !VirtualProtect(final_target_, 18, PAGE_EXECUTE_READWRITE, &protection)) return;
        std::memcpy(final_target_, final_original_, 18);
        FlushInstructionCache(GetCurrentProcess(), final_target_, 18);
        DWORD ignored{}; VirtualProtect(final_target_, 18, protection, &ignored);
    }
    active_module.store(nullptr, std::memory_order_release);
    while (active_calls.load(std::memory_order_acquire) != 0) {
        SwitchToThread();
    }
    VirtualFree(max_trampoline_, 0, MEM_RELEASE);
    max_trampoline_ = nullptr;
    max_target_ = nullptr;
    full_entity_support_ = false;
    release_12650_ = false;
    hooks_installed_ = false;
}

} // namespace utility::modules

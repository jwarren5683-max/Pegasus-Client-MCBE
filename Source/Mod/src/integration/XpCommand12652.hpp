#pragma once

#include "BedrockBuild.hpp"
#include "GameContext.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace utility::integration::xp_12652 {

// Minecraft 26.52's native XP command dispatches Player::addLevels through
// vtable +0x6C0 and Player::addExperience through vtable +0x6B8. The nearby
// command implementation is byte-gated before Loki uses either slot.
inline constexpr std::uintptr_t command_dispatch_rva = 0xB64139C;
inline constexpr std::size_t add_experience_slot = 0x6B8;
inline constexpr std::size_t add_levels_slot = 0x6C0;
inline constexpr std::array<std::byte, 39> command_dispatch_signature{
    std::byte{0x48},std::byte{0x8B},std::byte{0x80},std::byte{0xC0},std::byte{0x06},std::byte{0x00},std::byte{0x00},
    std::byte{0x4C},std::byte{0x89},std::byte{0xF9},std::byte{0xFF},std::byte{0x15},std::byte{0xFC},std::byte{0xA6},std::byte{0xAB},std::byte{0x03},
    std::byte{0x48},std::byte{0x8B},std::byte{0x45},std::byte{0xD8},std::byte{0x8B},std::byte{0x90},std::byte{0xE8},std::byte{0x00},std::byte{0x00},std::byte{0x00},
    std::byte{0x49},std::byte{0x8B},std::byte{0x07},std::byte{0x48},std::byte{0x8B},std::byte{0x80},std::byte{0xB8},std::byte{0x06},std::byte{0x00},std::byte{0x00},
    std::byte{0x4C},std::byte{0x89},std::byte{0xF9}
};

enum class ApplyResult { applied, unavailable };

[[nodiscard]] inline bool dispatch_signature_matches(const void* address) noexcept {
    return readable_game_memory(address, command_dispatch_signature.size()) &&
        std::memcmp(address, command_dispatch_signature.data(), command_dispatch_signature.size()) == 0;
}

[[nodiscard]] inline bool executable_address(const void* address) noexcept {
    MEMORY_BASIC_INFORMATION information{};
    if (!address || !VirtualQuery(address, &information, sizeof(information)) ||
        information.State != MEM_COMMIT || (information.Protect & PAGE_GUARD)) return false;
    const DWORD protection = information.Protect & 0xFF;
    return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}

[[nodiscard]] inline ApplyResult apply(void* player, int amount, bool levels) noexcept {
    const auto build = current_bedrock_build();
    if (!is_release_12652(build) || !build.image || !player ||
        !dispatch_signature_matches(reinterpret_cast<const std::byte*>(build.image) + command_dispatch_rva) ||
        !readable_game_memory(player,sizeof(void*))) return ApplyResult::unavailable;
    void** table{};
    std::memcpy(&table,player,sizeof(table));
    const auto required = (std::max)(add_experience_slot,add_levels_slot) + sizeof(void*);
    if (!readable_game_memory(table,required)) return ApplyResult::unavailable;
    void* target{};
    std::memcpy(&target,reinterpret_cast<const std::byte*>(table)+
        (levels?add_levels_slot:add_experience_slot),sizeof(target));
    if (!executable_address(target)) return ApplyResult::unavailable;
    reinterpret_cast<void(__fastcall*)(void*,int)>(target)(player,amount);
    return ApplyResult::applied;
}

} // namespace utility::integration::xp_12652

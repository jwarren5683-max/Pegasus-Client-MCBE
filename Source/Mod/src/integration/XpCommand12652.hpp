#pragma once

#include "BedrockBuild.hpp"
#include "GameContext.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace utility::integration::xp_12652 {

// Horion calls LocalPlayer::addExperience/addLevels. In Minecraft 26.52 the
// LocalPlayer overrides at vtable +0x6B8/+0x6C0 are deliberate no-op stubs,
// while the inherited Player implementations remain the functions used by the
// native XP command for real Player objects. Call those implementations
// directly so the old Horion behavior survives the modern LocalPlayer ABI.
inline constexpr std::uintptr_t add_experience_rva = 0x20AB80;
inline constexpr std::uintptr_t add_levels_rva = 0x20B140;
inline constexpr std::array<std::byte, 16> add_experience_signature{
    std::byte{0x55},std::byte{0x41},std::byte{0x57},std::byte{0x41},
    std::byte{0x56},std::byte{0x41},std::byte{0x54},std::byte{0x56},
    std::byte{0x57},std::byte{0x53},std::byte{0x48},std::byte{0x81},
    std::byte{0xEC},std::byte{0x00},std::byte{0x02},std::byte{0x00}
};
inline constexpr std::array<std::byte, 16> add_levels_signature{
    std::byte{0x55},std::byte{0x41},std::byte{0x57},std::byte{0x41},
    std::byte{0x56},std::byte{0x41},std::byte{0x55},std::byte{0x41},
    std::byte{0x54},std::byte{0x56},std::byte{0x57},std::byte{0x53},
    std::byte{0x48},std::byte{0x81},std::byte{0xEC},std::byte{0x08}
};

enum class ApplyResult { applied, unavailable };

template<std::size_t Size>
[[nodiscard]] inline bool signature_matches(const void* address,
    const std::array<std::byte,Size>& signature) noexcept {
    return readable_game_memory(address,signature.size()) &&
        std::memcmp(address,signature.data(),signature.size())==0;
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
    if (!is_release_12652(build)||!build.image||!player||
        !readable_game_memory(player,0xB28)) return ApplyResult::unavailable;
    auto* target=reinterpret_cast<std::byte*>(build.image)+(levels?add_levels_rva:add_experience_rva);
    if (!(levels?signature_matches(target,add_levels_signature):
        signature_matches(target,add_experience_signature))||!executable_address(target))
        return ApplyResult::unavailable;
    reinterpret_cast<void(__fastcall*)(void*,int)>(target)(player,amount);
    return ApplyResult::applied;
}

} // namespace utility::integration::xp_12652

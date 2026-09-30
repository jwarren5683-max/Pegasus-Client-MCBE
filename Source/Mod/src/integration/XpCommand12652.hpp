#pragma once

#include "BedrockBuild.hpp"
#include "GameContext.hpp"
#include "ServerSafety.hpp"

#include <algorithm>
#include <array>
#include <atomic>
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

enum class RequestResult { queued, unavailable, busy };
enum class ApplyResult { none, applied, rejected };
struct Request { int amount{}; bool levels{}; };

inline constexpr std::uint64_t present_bit = std::uint64_t{1} << 63;
inline constexpr std::uint64_t levels_bit = std::uint64_t{1} << 62;
inline std::atomic<std::uint64_t> pending{};

[[nodiscard]] inline constexpr std::uint64_t encode(Request request) noexcept {
    return present_bit | (request.levels ? levels_bit : 0) |
        static_cast<std::uint32_t>(request.amount);
}

[[nodiscard]] inline constexpr Request decode(std::uint64_t encoded) noexcept {
    return {static_cast<int>(static_cast<std::int32_t>(encoded)), (encoded & levels_bit) != 0};
}

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

[[nodiscard]] inline RequestResult request(int amount, bool levels) noexcept {
    const auto build = current_bedrock_build();
    if (!is_release_12652(build) || !server_safety::local_world()) return RequestResult::unavailable;
    std::uint64_t empty{};
    return pending.compare_exchange_strong(empty, encode({amount,levels}),
        std::memory_order_acq_rel) ? RequestResult::queued : RequestResult::busy;
}

[[nodiscard]] inline ApplyResult apply_pending(void* authoritative_player) noexcept {
    const auto encoded = pending.load(std::memory_order_acquire);
    if (!(encoded & present_bit)) return ApplyResult::none;
    const auto build = current_bedrock_build();
    if (!is_release_12652(build) || !build.image || !authoritative_player ||
        !dispatch_signature_matches(reinterpret_cast<const std::byte*>(build.image) + command_dispatch_rva) ||
        !readable_game_memory(authoritative_player,sizeof(void*))) {
        pending.store(0,std::memory_order_release);
        return ApplyResult::rejected;
    }
    void** table{};
    std::memcpy(&table,authoritative_player,sizeof(table));
    const auto required = (std::max)(add_experience_slot,add_levels_slot) + sizeof(void*);
    if (!readable_game_memory(table,required)) {
        pending.store(0,std::memory_order_release);
        return ApplyResult::rejected;
    }
    const auto request_value=decode(encoded);
    void* target{};
    std::memcpy(&target,reinterpret_cast<const std::byte*>(table)+
        (request_value.levels?add_levels_slot:add_experience_slot),sizeof(target));
    if (!executable_address(target)) {
        pending.store(0,std::memory_order_release);
        return ApplyResult::rejected;
    }
    // Consume exactly once on the integrated server's authoritative player
    // tick, before the normal tick publishes the resulting XP state.
    if (pending.exchange(0,std::memory_order_acq_rel) != encoded) return ApplyResult::none;
    reinterpret_cast<void(__fastcall*)(void*,int)>(target)(authoritative_player,request_value.amount);
    return ApplyResult::applied;
}

inline void reset() noexcept { pending.store(0,std::memory_order_release); }

} // namespace utility::integration::xp_12652

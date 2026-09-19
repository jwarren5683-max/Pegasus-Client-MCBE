#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace utility::integration::world_seed_12650 {

using Byte = unsigned char;

// Exact Microsoft.MinecraftUWP 1.26.5101.0 targets. BlockSource slot +0x160
// returns Level*. Level::getSeed uses a hidden return buffer for LevelSeed64;
// treating the returned buffer address as the value was the original bug.
inline constexpr std::size_t block_source_get_level_slot = 0x160;
inline constexpr std::uintptr_t block_source_get_level_rva = 0x3462F0;
inline constexpr std::uintptr_t level_get_seed_rva = 0x11788A0;
inline constexpr std::size_t level_data_vtable_slot = 0x560;

inline constexpr std::array<Byte, 5> block_source_get_level_signature{
    0x48,0x8B,0x41,0x20,0xC3
};
inline constexpr std::array<Byte, 32> level_get_seed_signature{
    0x56,0x48,0x83,0xEC,0x20,0x48,0x89,0xD6,0x48,0x8B,0x01,0x48,0x8B,0x80,0x60,0x05,
    0,0,0xFF,0x15,0xF0,0x31,0xF8,0x0D,0x48,0x8D,0x15,0x81,0x3C,0xBB,0x10,0x48
};

struct LevelSeed64Abi { std::uint64_t value{}; };
static_assert(sizeof(LevelSeed64Abi) == sizeof(std::uint64_t));

[[nodiscard]] inline bool returned_expected_buffer(const LevelSeed64Abi* returned,
    const LevelSeed64Abi* expected) noexcept {
    return returned != nullptr && returned == expected;
}

} // namespace utility::integration::world_seed_12650

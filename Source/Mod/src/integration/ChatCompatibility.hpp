#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace utility::integration::chat_compat {

using Byte = unsigned char;

struct Profile {
    std::uint32_t timestamp{};
    std::uint32_t image_size{};
    std::uintptr_t submit_rva{};
    std::uintptr_t display_rva{};
    std::uintptr_t field_signature_rva{};
    std::uintptr_t send_signature_rva{};
    std::array<Byte, 15> submit_signature{};
    std::array<Byte, 7> field_signature{};
    std::array<Byte, 13> send_signature{};
};

inline constexpr Profile release_12645{
    0x6A8378BA, 0x12888000, 0x4B41070, 0x16DA850, 0x4B4109D, 0x4B41386,
    {0x55,0x41,0x57,0x41,0x56,0x56,0x57,0x53,0x48,0x81,0xEC,0xD8,0,0,0},
    {0x48,0x8D,0xB9,0x60,0x0D,0,0},
    {0xE8,0xA5,0x71,0xF0,0xFC,0,0,0,0,0,0,0,0}
};

// Extracted from the mapped Microsoft.MinecraftUWP 1.26.5101.0 image.  The
// final fingerprint covers both native send branches, not merely a common
// function prologue.
inline constexpr Profile release_12650{
    0x6AA482FD, 0x12C01000, 0x4DE18B0, 0x155F190, 0x4DE18DD, 0x4DE1C22,
    {0x55,0x41,0x57,0x41,0x56,0x56,0x57,0x53,0x48,0x81,0xEC,0xD8,0,0,0},
    {0x48,0x8D,0xB9,0x60,0x0D,0,0},
    {0xE8,0x99,0xBB,0xAA,0xFC,0xEB,0x08,0x48,0x89,0xFA,0xE8,0x7F,0xB5}
};

inline constexpr std::array<Byte, 32> display_12650_signature{
    0x55,0x56,0x53,0x48,0x81,0xEC,0x90,0x02,0,0,0x48,0x8D,0xAC,0x24,0x80,0,0,0,
    0x48,0xC7,0x85,0x08,0x02,0,0,0xFE,0xFF,0xFF,0xFF,0x44,0x88,0xCB
};
inline constexpr std::uintptr_t controller_12650_signature_rva = 0x4DE1C31;
inline constexpr std::array<Byte, 11> controller_12650_signature{
    0x48,0x8B,0x9E,0x48,0x0D,0,0,0x48,0x8B,0x43,0x48
};

[[nodiscard]] inline const Profile* select(std::uint32_t timestamp, std::uint32_t image_size) noexcept {
    if (timestamp == release_12645.timestamp && image_size == release_12645.image_size) return &release_12645;
    if (timestamp == release_12650.timestamp && image_size == release_12650.image_size) return &release_12650;
    return nullptr;
}

[[nodiscard]] inline bool matches_fragments(const Byte* submit, const Byte* field, const Byte* send,
    const Profile& profile) noexcept {
    if (!submit || !field || !send) return false;
    const std::size_t send_size = profile.timestamp == release_12645.timestamp ? 5 : profile.send_signature.size();
    return std::memcmp(submit, profile.submit_signature.data(), profile.submit_signature.size()) == 0 &&
        std::memcmp(field, profile.field_signature.data(), profile.field_signature.size()) == 0 &&
        std::memcmp(send, profile.send_signature.data(), send_size) == 0;
}

[[nodiscard]] inline bool matches(const Byte* image, const Profile& profile) noexcept {
    if (!image) return false;
    return matches_fragments(image + profile.submit_rva, image + profile.field_signature_rva,
        image + profile.send_signature_rva, profile);
}

} // namespace utility::integration::chat_compat

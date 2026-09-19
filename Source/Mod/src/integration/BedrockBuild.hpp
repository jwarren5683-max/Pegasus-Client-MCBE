#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>

namespace utility::integration {

enum class BedrockBuildKind {
    unsupported,
    release_12645,
    release_12650,
};

struct BedrockBuildInfo final {
    HMODULE image{};
    std::uint32_t timestamp{};
    std::uint32_t image_size{};
    BedrockBuildKind kind{BedrockBuildKind::unsupported};
};

inline constexpr std::uint32_t release_12645_timestamp = 0x6A8378BA;
inline constexpr std::uint32_t release_12645_image_size = 0x12888000;
inline constexpr std::uint32_t release_12650_timestamp = 0x6AA482FD;
inline constexpr std::uint32_t release_12650_image_size = 0x12C01000;

[[nodiscard]] constexpr BedrockBuildKind classify_bedrock_build(std::uint32_t timestamp,
                                                                 std::uint32_t image_size) noexcept {
    if (timestamp == release_12645_timestamp && image_size == release_12645_image_size)
        return BedrockBuildKind::release_12645;
    if (timestamp == release_12650_timestamp && image_size == release_12650_image_size)
        return BedrockBuildKind::release_12650;
    return BedrockBuildKind::unsupported;
}

[[nodiscard]] inline BedrockBuildInfo current_bedrock_build() noexcept {
    BedrockBuildInfo result{};
    result.image = GetModuleHandleW(nullptr);
    if (result.image == nullptr) return result;

    const auto* base = reinterpret_cast<const std::byte*>(result.image);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x100000) {
        result.image = nullptr;
        return result;
    }

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt->OptionalHeader.SizeOfImage < 0x100000 || nt->OptionalHeader.SizeOfImage > 0x40000000) {
        result.image = nullptr;
        return result;
    }

    result.timestamp = nt->FileHeader.TimeDateStamp;
    result.image_size = nt->OptionalHeader.SizeOfImage;
    result.kind = classify_bedrock_build(result.timestamp, result.image_size);
    return result;
}

[[nodiscard]] inline bool is_release_12645(const BedrockBuildInfo& build) noexcept {
    return build.kind == BedrockBuildKind::release_12645;
}

[[nodiscard]] inline bool is_release_12650(const BedrockBuildInfo& build) noexcept {
    return build.kind == BedrockBuildKind::release_12650;
}

} // namespace utility::integration

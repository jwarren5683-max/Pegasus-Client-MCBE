#pragma once

#include <Windows.h>

#include <cstdint>

namespace utility::integration {

struct BedrockBuildInfo final {
    HMODULE image{};
    std::uint32_t timestamp{};
    std::uint32_t image_size{};
};

inline constexpr std::uint32_t legacy_12645_timestamp = 0x6A8378BA;
inline constexpr std::uint32_t legacy_12645_image_size = 0x12888000;

[[nodiscard]] inline BedrockBuildInfo current_bedrock_build() noexcept {
    BedrockBuildInfo result{};
    result.image = GetModuleHandleW(nullptr);
    if (result.image == nullptr) return result;

    const auto* base = reinterpret_cast<const std::byte*>(result.image);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
        result.image = nullptr;
        return result;
    }

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        result.image = nullptr;
        return result;
    }

    result.timestamp = nt->FileHeader.TimeDateStamp;
    result.image_size = nt->OptionalHeader.SizeOfImage;
    return result;
}

[[nodiscard]] inline bool is_legacy_12645_build(const BedrockBuildInfo& build) noexcept {
    return build.image != nullptr &&
        build.timestamp == legacy_12645_timestamp &&
        build.image_size == legacy_12645_image_size;
}

} // namespace utility::integration

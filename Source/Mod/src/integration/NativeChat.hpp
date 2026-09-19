#pragma once

#include "ChatCompatibility.hpp"
#include "GameContext.hpp"

#include <Windows.h>
#include <cstddef>
#include <cstring>
#include <string>

namespace utility::integration::native_chat {

using Byte = unsigned char;

// Explicit release MSVC layouts. Never pass this DLL's Debug std::string ABI
// across the Minecraft boundary.
struct NativeString {
    union { char text[16]; const char* pointer; } data{};
    std::size_t size{}, capacity{15};
    NativeString() = default;
    explicit NativeString(const char* value) noexcept {
        size = value ? std::strlen(value) : 0;
        if (size < 16) {
            if (size) std::memcpy(data.text, value, size);
        } else {
            data.pointer = value;
            capacity = size;
        }
    }
};
static_assert(sizeof(NativeString) == 32);
struct OptionalString { NativeString value; bool present{}; Byte padding[7]{}; };
static_assert(sizeof(OptionalString) == 40);

struct GuiDataHandle {
    void* validity{};
    void* reference_count{};
    void* gui_data{};
};
static_assert(sizeof(GuiDataHandle) == 24);

[[nodiscard]] inline bool executable_in_image(const void* address, const Byte* image,
    std::size_t image_size) noexcept {
    if (!address || !image) return false;
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    const auto begin = reinterpret_cast<std::uintptr_t>(image);
    if (value < begin || value >= begin + image_size) return false;
    MEMORY_BASIC_INFORMATION memory{};
    if (!VirtualQuery(address, &memory, sizeof(memory))) return false;
    constexpr DWORD executable = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return memory.State == MEM_COMMIT && !(memory.Protect & PAGE_GUARD) && (memory.Protect & executable);
}

#if defined(_MSC_VER)
inline bool acquire_gui_data(void* client, GuiDataHandle& output, const Byte* image,
    std::size_t image_size) noexcept {
    __try {
        if (!readable_game_memory(client, sizeof(void*))) return false;
        auto** table = *reinterpret_cast<void***>(client);
        if (!readable_game_memory(table, 0x6D8)) return false;
        auto* target = table[0x6D0 / sizeof(void*)];
        if (!executable_in_image(target, image, image_size)) return false;
        using GetGuiData = void(__fastcall*)(void*, GuiDataHandle*);
        reinterpret_cast<GetGuiData>(target)(client, &output);
        return output.validity && readable_game_memory(output.validity, 1) &&
            *static_cast<const Byte*>(output.validity) != 0 && output.reference_count &&
            readable_game_memory(output.reference_count, 16) &&
            readable_game_memory(output.gui_data, sizeof(void*));
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

inline void release_one(void* reference_count, const Byte* image, std::size_t image_size) noexcept {
    __try {
        if (!readable_game_memory(reference_count, 16)) return;
        auto* counts = static_cast<LONG*>(reference_count);
        if (InterlockedDecrement(counts + 2) == 0) {
            auto** table = *reinterpret_cast<void***>(reference_count);
            if (!readable_game_memory(table, 16) || !executable_in_image(table[0], image, image_size)) return;
            reinterpret_cast<void(__fastcall*)(void*)>(table[0])(reference_count);
            if (InterlockedDecrement(counts + 3) == 0) {
                table = *reinterpret_cast<void***>(reference_count);
                if (!readable_game_memory(table, 16) || !executable_in_image(table[1], image, image_size)) return;
                reinterpret_cast<void(__fastcall*)(void*)>(table[1])(reference_count);
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
}

inline bool invoke_display(void* gui_data, const NativeString* message,
    const OptionalString* source, const Byte* image) noexcept {
    __try {
        using Display = void(__fastcall*)(void*, const NativeString*, const OptionalString*, bool);
        reinterpret_cast<Display>(const_cast<Byte*>(image) + chat_compat::release_12650.display_rva)(
            gui_data, message, source, false);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
#endif

// owner is either a LocalPlayer (+0xD70 -> IClientInstance) or an already
// verified ChatScreenController model (+0x58 -> IClientInstance).
inline bool display_12650(void* owner, std::size_t client_offset, const char* text) noexcept {
#if !defined(_MSC_VER)
    (void)owner; (void)client_offset; (void)text; return false;
#else
    const auto build = current_bedrock_build();
    auto* image = reinterpret_cast<Byte*>(GetModuleHandleW(nullptr));
    if (!owner || !text || !image || !is_release_12650(build) ||
        std::memcmp(image + chat_compat::release_12650.display_rva,
            chat_compat::display_12650_signature.data(), chat_compat::display_12650_signature.size()) != 0)
        return false;
    void* client{};
    if (!readable_game_memory(static_cast<Byte*>(owner) + client_offset, sizeof(client))) return false;
    std::memcpy(&client, static_cast<Byte*>(owner) + client_offset, sizeof(client));
    GuiDataHandle handle{};
    if (!acquire_gui_data(client, handle, image, build.image_size)) return false;
    const NativeString message(text);
    const OptionalString source{};
    const bool shown = invoke_display(handle.gui_data, &message, &source, image);
    // The returned NonOwnerPointer owns two references sharing this control
    // block. This is the exact cleanup emitted at each verified 26.50 caller.
    release_one(handle.reference_count, image, build.image_size);
    release_one(handle.reference_count, image, build.image_size);
    return shown;
#endif
}

} // namespace utility::integration::native_chat

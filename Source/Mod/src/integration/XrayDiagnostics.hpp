#pragma once

#include "../framework/Logger.hpp"
#include "BedrockBuild.hpp"

#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string_view>

namespace utility::integration {

namespace xray_diagnostics {

inline bool readable(std::uintptr_t p, std::size_t size) noexcept {
    if (!p || !size || p + size < p) return false;
    MEMORY_BASIC_INFORMATION m{};
    if (!VirtualQuery(reinterpret_cast<void*>(p), &m, sizeof(m)) || m.State != MEM_COMMIT ||
        (m.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    return p >= reinterpret_cast<std::uintptr_t>(m.BaseAddress) &&
        p + size <= reinterpret_cast<std::uintptr_t>(m.BaseAddress) + m.RegionSize;
}

template<class T>
inline T get(std::uintptr_t p) noexcept {
    T value{};
    if (readable(p, sizeof(T))) std::memcpy(&value, reinterpret_cast<void*>(p), sizeof(T));
    return value;
}

inline bool block_name(std::uintptr_t type, char (&out)[160], std::size_t& length) noexcept {
    const auto text = type + 0xE8;
    if (!readable(text, 32)) return false;
    length = get<std::size_t>(text + 16);
    const auto capacity = get<std::size_t>(text + 24);
    if (length < 11 || length >= sizeof(out) || capacity < length || capacity > 4096) return false;
    const auto data = capacity < 16 ? text : get<std::uintptr_t>(text);
    if (!readable(data, length)) return false;
    std::memcpy(out, reinterpret_cast<void*>(data), length);
    out[length] = 0;
    return std::string_view(out, length).starts_with("minecraft:");
}

struct CandidateResult {
    std::size_t entries{};
    std::size_t readable_objects{};
    std::size_t same_vtable{};
    std::size_t named_blocks{};
    std::size_t ore_names{};
    bool air{};
    bool stone{};
    bool bad_object{};
    bool bad_shape{};
};

inline bool is_ore(std::string_view id) noexcept {
    constexpr std::string_view ores[]{
        "minecraft:coal_ore", "minecraft:deepslate_coal_ore",
        "minecraft:copper_ore", "minecraft:deepslate_copper_ore",
        "minecraft:iron_ore", "minecraft:deepslate_iron_ore",
        "minecraft:gold_ore", "minecraft:deepslate_gold_ore",
        "minecraft:redstone_ore", "minecraft:lit_redstone_ore",
        "minecraft:deepslate_redstone_ore", "minecraft:lit_deepslate_redstone_ore",
        "minecraft:lapis_ore", "minecraft:deepslate_lapis_ore",
        "minecraft:diamond_ore", "minecraft:deepslate_diamond_ore",
        "minecraft:emerald_ore", "minecraft:deepslate_emerald_ore",
        "minecraft:nether_gold_ore", "minecraft:quartz_ore", "minecraft:ancient_debris"};
    for (auto ore : ores) if (id == ore) return true;
    return false;
}

inline CandidateResult inspect_candidate(std::uintptr_t vector, std::uintptr_t expected_vtable) noexcept {
    CandidateResult result{};
    const auto begin = get<std::uintptr_t>(vector);
    const auto end = get<std::uintptr_t>(vector + 8);
    if (end <= begin || (end - begin) % 8 || !readable(begin, end - begin)) return result;
    result.entries = (end - begin) / 8;
    for (auto pos = begin; pos < end; pos += 8) {
        const auto object = get<std::uintptr_t>(pos);
        if (!readable(object, 0x80)) { result.bad_object = true; break; }
        ++result.readable_objects;
        if (get<std::uintptr_t>(object) != expected_vtable) { result.bad_object = true; break; }
        ++result.same_vtable;
        const auto block = get<std::uintptr_t>(object + 8);
        if (!readable(block, 0x70)) continue;
        const auto type = get<std::uintptr_t>(block + 0x68);
        char name[160]{}; std::size_t length{};
        if (!block_name(type, name, length)) continue;
        ++result.named_blocks;
        const std::string_view id(name, length);
        if (is_ore(id)) ++result.ore_names;
        const int shape = get<int>(object + 16);
        if (shape < -1 || shape > 512) { result.bad_shape = true; break; }
        result.air |= id == "minecraft:air" && shape == -1;
        result.stone |= id == "minecraft:stone" && shape == 0;
    }
    return result;
}

inline void run() noexcept {
    const auto build = current_bedrock_build();
    if (!is_release_12650(build) || build.image == nullptr) return;

    const auto base = reinterpret_cast<std::uintptr_t>(build.image);
    const auto* bytes = reinterpret_cast<const unsigned char*>(base);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;

    std::size_t vector_shapes = 0;
    std::size_t readable_vectors = 0;
    std::size_t plausible_objects = 0;
    std::size_t inspected = 0;
    std::uintptr_t best_vector = 0;
    std::uintptr_t best_vtable = 0;
    CandidateResult best{};

    const auto* sections = IMAGE_FIRST_SECTION(nt);
    for (unsigned index = 0; index < nt->FileHeader.NumberOfSections; ++index) {
        const auto& section = sections[index];
        if (section.Characteristics & IMAGE_SCN_MEM_EXECUTE) continue;
        const auto start = static_cast<std::uintptr_t>(section.VirtualAddress);
        const auto size = static_cast<std::uintptr_t>(section.Misc.VirtualSize);
        if (start >= build.image_size || size < 16) continue;
        const auto limit = (std::min)(size, static_cast<std::uintptr_t>(build.image_size) - start);
        for (std::uintptr_t offset = 0; offset + 16 <= limit; offset += 8) {
            const auto address = base + start + offset;
            const auto begin = get<std::uintptr_t>(address);
            const auto end = get<std::uintptr_t>(address + 8);
            if (!begin || end <= begin || (end - begin) % 8) continue;
            ++vector_shapes;
            const auto count = (end - begin) / 8;
            if (count < 64 || count > 20000 || !readable(begin, (std::min<std::uintptr_t>)(end - begin, 256))) continue;
            ++readable_vectors;
            const auto object = get<std::uintptr_t>(begin);
            const auto table = get<std::uintptr_t>(object);
            if (!object || !table || table < base || table >= base + build.image_size || !readable(object, 0x80)) continue;
            ++plausible_objects;
            const auto candidate = inspect_candidate(address, table);
            ++inspected;
            if (candidate.named_blocks > best.named_blocks ||
                (candidate.named_blocks == best.named_blocks && candidate.ore_names > best.ore_names)) {
                best = candidate;
                best_vector = address;
                best_vtable = table;
            }
        }
    }

    std::ostringstream message;
    message << "x-ray diagnostic 26.50: vector_shapes=" << vector_shapes
            << ", readable_vectors=" << readable_vectors
            << ", plausible_objects=" << plausible_objects
            << ", inspected=" << inspected
            << ", best_entries=" << best.entries
            << ", best_readable_objects=" << best.readable_objects
            << ", best_same_vtable=" << best.same_vtable
            << ", best_named_blocks=" << best.named_blocks
            << ", best_ores=" << best.ore_names
            << ", air=" << (best.air ? "yes" : "no")
            << ", stone=" << (best.stone ? "yes" : "no")
            << ", bad_object=" << (best.bad_object ? "yes" : "no")
            << ", bad_shape=" << (best.bad_shape ? "yes" : "no")
            << ", vector_rva=0x" << std::hex << (best_vector ? best_vector - base : 0)
            << ", vtable_rva=0x" << (best_vtable ? best_vtable - base : 0);
    Logger::instance().info(message.str());
}

} // namespace xray_diagnostics

inline void run_xray_12650_diagnostics_async() noexcept {
    const auto thread = CreateThread(nullptr, 0, [](void*) -> DWORD {
        // Give Bedrock a moment to finish post-injection resource initialization.
        Sleep(1500);
        xray_diagnostics::run();
        return 0;
    }, nullptr, 0, nullptr);
    if (thread) CloseHandle(thread);
}

} // namespace utility::integration

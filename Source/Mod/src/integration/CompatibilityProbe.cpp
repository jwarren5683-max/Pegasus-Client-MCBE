#include "CompatibilityProbe.hpp"

#include "BedrockBuild.hpp"
#include "../framework/Logger.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace utility::integration {
namespace {

struct Pattern {
    std::string_view name;
    std::string_view text;
    bool pointer_references{};
};

constexpr Pattern patterns[] = {
    {"Reach pick range legacy", "56 57 48 83 EC 28 8B 02 83 F8 01 74 0F 83 F8 03", true},
    {"Reach maximum range legacy", "56 57 48 83 EC 28 48 8B 79 08 48 8D 4F 08", true},
    {"GameMode getPickRange candidate", "48 83 EC 28 45 84 C0 74 25", true},
    {"Actor component lookup", "48 8B 81 E8 01 00 00 C3", true},
    {"HitResult actor resolver", "48 83 EC 48 48 8D 51 38 48 8D 4C 24 28", true},
    {"Actor isAlive helper", "48 83 EC 28 80 B9 69 02 00 00", true},
    {"AntiKnockback target", "55 41 57 41 56 56 57 53 48 81 EC D8 01 00 00", true},
    {"Splash loader", "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 88 05 00 00", true},
    {"Airjump target", "55 41 57 41 56 56 57 53 48 81 EC 58 01 00 00", true},
    {"Ejection target", "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC C8 00 00 00", true},
    {"Chat submit", "55 41 57 41 56 56 57 53 48 81 EC D8 00 00 00", true},
    {"Xray rebuild", "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 88 00 00 00", true},
    {"StateVector component hash", "DA BA 91 3C C9 0E", false},
    {"MoveInput component hash", "DA BA 2E CD 8B 46", false},
    {"AABBShape component hash", "DA BA F2 C9 10 1B", false},
    {"Attributes component hash", "DA BA 13 06 3B FD", false},
};

struct ParsedPattern {
    std::vector<unsigned char> bytes;
    std::vector<bool> exact;
};

ParsedPattern parse(std::string_view text) {
    ParsedPattern result;
    for (std::size_t index = 0; index < text.size();) {
        while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index]))) ++index;
        if (index >= text.size()) break;
        const auto end = text.find(' ', index);
        const auto token = text.substr(index, end == std::string_view::npos ? text.size() - index : end - index);
        if (token == "?" || token == "??") {
            result.bytes.push_back(0);
            result.exact.push_back(false);
        } else {
            unsigned value{};
            std::istringstream stream{std::string(token)};
            stream >> std::hex >> value;
            result.bytes.push_back(static_cast<unsigned char>(value));
            result.exact.push_back(true);
        }
        if (end == std::string_view::npos) break;
        index = end + 1;
    }
    return result;
}

bool readable(const void* address, std::size_t size) noexcept {
    if (!address || !size) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(address, &info, sizeof(info)) || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(address);
    const auto end = begin + size;
    const auto region_end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    return end >= begin && end <= region_end;
}

std::vector<std::uintptr_t> find(const std::byte* begin, std::size_t size,
                                 const ParsedPattern& pattern) {
    std::vector<std::uintptr_t> matches;
    if (pattern.bytes.empty() || pattern.bytes.size() > size || !readable(begin, size)) return matches;
    const auto* bytes = reinterpret_cast<const unsigned char*>(begin);
    for (std::size_t at = 0; at + pattern.bytes.size() <= size; ++at) {
        bool match = true;
        for (std::size_t index = 0; index < pattern.bytes.size(); ++index) {
            if (pattern.exact[index] && bytes[at + index] != pattern.bytes[index]) {
                match = false;
                break;
            }
        }
        if (match) matches.push_back(at);
    }
    return matches;
}

void log_matches(const Pattern& source, const std::vector<std::uintptr_t>& matches) {
    std::ostringstream message;
    message << "26.50 probe: " << source.name << " matches=" << matches.size();
    const auto shown = (std::min<std::size_t>)(matches.size(), 12);
    for (std::size_t index = 0; index < shown; ++index)
        message << " rva=0x" << std::uppercase << std::hex << matches[index];
    if (matches.size() > shown) message << " ...";
    Logger::instance().info(message.str());
}

std::vector<std::uintptr_t> find_pointer_references(const std::byte* base,
                                                    const IMAGE_NT_HEADERS64* nt,
                                                    std::uintptr_t target,
                                                    std::size_t image_size) {
    std::vector<std::uintptr_t> matches;
    const auto* sections = IMAGE_FIRST_SECTION(nt);
    for (unsigned index = 0; index < nt->FileHeader.NumberOfSections; ++index) {
        const auto& section = sections[index];
        if (section.Characteristics & IMAGE_SCN_MEM_EXECUTE) continue;
        const std::size_t start = (std::min<std::size_t>)(section.VirtualAddress, image_size);
        const std::size_t size = (std::min<std::size_t>)(section.Misc.VirtualSize, image_size - start);
        if (size < sizeof(target) || !readable(base + start, size)) continue;
        for (std::size_t offset = 0; offset + sizeof(target) <= size; offset += alignof(void*)) {
            std::uintptr_t candidate{};
            std::memcpy(&candidate, base + start + offset, sizeof(candidate));
            if (candidate == target) matches.push_back(start + offset);
        }
    }
    return matches;
}

void log_pointer_references(const Pattern& source, const std::vector<std::uintptr_t>& references) {
    std::ostringstream message;
    message << "26.50 probe: " << source.name << " absolute-pointer-refs=" << references.size();
    const auto shown = (std::min<std::size_t>)(references.size(), 24);
    for (std::size_t index = 0; index < shown; ++index)
        message << " rva=0x" << std::uppercase << std::hex << references[index];
    if (references.size() > shown) message << " ...";
    Logger::instance().info(message.str());
}

} // namespace

void run_compatibility_probe() noexcept {
    const auto build = current_bedrock_build();
    if (!is_release_12650(build)) return;

    const auto* base = reinterpret_cast<const std::byte*>(build.image);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    const auto* sections = IMAGE_FIRST_SECTION(nt);
    for (const auto& source : patterns) {
        std::vector<std::uintptr_t> matches;
        const auto parsed = parse(source.text);
        for (unsigned index = 0; index < nt->FileHeader.NumberOfSections; ++index) {
            const auto& section = sections[index];
            if (!(section.Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
            const std::size_t size = (std::min<std::size_t>)(section.Misc.VirtualSize,
                build.image_size - (std::min<std::size_t>)(section.VirtualAddress, build.image_size));
            for (const auto offset : find(base + section.VirtualAddress, size, parsed))
                matches.push_back(section.VirtualAddress + offset);
        }
        log_matches(source, matches);
        if (source.pointer_references && matches.size() == 1)
            log_pointer_references(source, find_pointer_references(base, nt,
                reinterpret_cast<std::uintptr_t>(base) + matches.front(), build.image_size));
    }
}

} // namespace utility::integration


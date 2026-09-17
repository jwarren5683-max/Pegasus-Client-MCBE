#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Pattern {
    const char* name;
    const char* bytes;
    bool list_pointer_references{};
};

struct ParsedPattern {
    std::string name;
    std::vector<unsigned char> bytes;
    std::vector<bool> exact;
    bool list_pointer_references{};
};

constexpr Pattern patterns[] = {
    {"Reach::getPickRange legacy prologue", "56 57 48 83 EC 28 8B 02 83 F8 01 74 0F 83 F8 03", true},
    {"Reach::getMaxPickRange legacy prologue", "56 57 48 83 EC 28 48 8B 79 08 48 8D 4F 08", true},
    {"GameMode::getPickRange current public signature", "48 83 EC 28 45 84 C0 74 25", true},
    {"Actor component lookup", "48 8B 81 E8 01 00 00 C3", true},
    {"HitResult actor resolver", "48 83 EC 48 48 8D 51 38 48 8D 4C 24 28", true},
    {"Actor::isAlive helper", "48 83 EC 28 80 B9 69 02 00 00", true},
    {"AntiKnockback::applyKnockback", "55 41 57 41 56 56 57 53 48 81 EC D8 01 00 00", true},
    {"Splash loader", "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 88 05 00 00", true},
    {"Airjump hook target", "55 41 57 41 56 56 57 53 48 81 EC 58 01 00 00", true},
    {"Ejection hook target", "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC C8 00 00 00", true},
    {"Chat submit", "55 41 57 41 56 56 57 53 48 81 EC D8 00 00 00", true},
    {"Xray rebuild", "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 88 00 00 00", true},
    {"Player inventory access", "48 8B 91 ? ? ? ? 80 BA ? ? ? ? ? 75 ? 48 8B 8A ? ? ? ? 8B 52 ? 48 8B 01", false},
    {"StateVector tryGet", "DA BA 91 3C C9 0E", false},
    {"MoveInput tryGet", "DA BA 2E CD 8B 46", false},
    {"AABBShape tryGet", "DA BA F2 C9 10 1B", false},
    {"Attributes tryGet", "DA BA 13 06 3B FD", false},
};

bool readable(DWORD protect) {
    if ((protect & PAGE_GUARD) != 0 || (protect & PAGE_NOACCESS) != 0) return false;
    const DWORD base = protect & 0xFF;
    return base == PAGE_READONLY || base == PAGE_READWRITE || base == PAGE_WRITECOPY ||
        base == PAGE_EXECUTE || base == PAGE_EXECUTE_READ || base == PAGE_EXECUTE_READWRITE ||
        base == PAGE_EXECUTE_WRITECOPY;
}

ParsedPattern parse(const Pattern& source) {
    ParsedPattern result{source.name, {}, {}, source.list_pointer_references};
    std::string text = source.bytes;
    for (std::size_t index = 0; index < text.size();) {
        while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index]))) ++index;
        if (index >= text.size()) break;
        const auto end = text.find(' ', index);
        const std::string token = text.substr(index, end == std::string::npos ? end : end - index);
        if (token == "?" || token == "??") {
            result.bytes.push_back(0);
            result.exact.push_back(false);
        } else {
            result.bytes.push_back(static_cast<unsigned char>(std::strtoul(token.c_str(), nullptr, 16)));
            result.exact.push_back(true);
        }
        if (end == std::string::npos) break;
        index = end + 1;
    }
    return result;
}

std::vector<std::size_t> find_all(const std::vector<unsigned char>& image,
                                  const std::vector<unsigned char>& valid,
                                  const ParsedPattern& pattern) {
    std::vector<std::size_t> matches;
    if (pattern.bytes.empty() || pattern.bytes.size() > image.size()) return matches;
    for (std::size_t at = 0; at + pattern.bytes.size() <= image.size(); ++at) {
        bool match = true;
        for (std::size_t index = 0; index < pattern.bytes.size(); ++index) {
            if (!valid[at + index] || (pattern.exact[index] && image[at + index] != pattern.bytes[index])) {
                match = false;
                break;
            }
        }
        if (match) matches.push_back(at);
    }
    return matches;
}

std::vector<std::size_t> pointer_references(const std::vector<unsigned char>& image,
                                            const std::vector<unsigned char>& valid,
                                            std::uintptr_t pointer) {
    std::vector<std::size_t> matches;
    std::array<unsigned char, sizeof(pointer)> bytes{};
    std::memcpy(bytes.data(), &pointer, sizeof(pointer));
    for (std::size_t at = 0; at + bytes.size() <= image.size(); at += alignof(void*)) {
        bool match = true;
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            if (!valid[at + index] || image[at + index] != bytes[index]) {
                match = false;
                break;
            }
        }
        if (match) matches.push_back(at);
    }
    return matches;
}

bool process_module(HANDLE process, std::uintptr_t& base, std::wstring& path) {
    std::array<HMODULE, 2048> modules{};
    DWORD needed{};
    if (!K32EnumProcessModulesEx(process, modules.data(), static_cast<DWORD>(sizeof(modules)), &needed,
                                 LIST_MODULES_ALL)) return false;
    const auto count = (std::min<std::size_t>)(modules.size(), needed / sizeof(HMODULE));
    for (std::size_t index = 0; index < count; ++index) {
        wchar_t name[MAX_PATH]{};
        if (!K32GetModuleBaseNameW(process, modules[index], name, MAX_PATH) ||
            _wcsicmp(name, L"Minecraft.Windows.exe") != 0) continue;
        wchar_t filename[32768]{};
        const DWORD length = K32GetModuleFileNameExW(process, modules[index], filename,
                                                     static_cast<DWORD>(std::size(filename)));
        base = reinterpret_cast<std::uintptr_t>(modules[index]);
        if (length) path.assign(filename, length);
        return true;
    }
    return false;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    DWORD requested_pid{};
    const wchar_t* dump_path{};
    if(argc!=1){
        if((argc!=3&&argc!=5)||_wcsicmp(argv[1],L"--pid")!=0||(argc==5&&_wcsicmp(argv[3],L"--dump-image")!=0)){std::fprintf(stderr,"Usage: BedrockCompatibilityScanner [--pid PID [--dump-image NEW_FILE]]\n");return 1;}
        wchar_t* tail{};const auto value=std::wcstoul(argv[2],&tail,10);
        if(!value||!tail||*tail){std::fprintf(stderr,"Invalid process ID.\n");return 1;}requested_pid=value;
        if(argc==5)dump_path=argv[4];
    }
    const HANDLE processes = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (processes == INVALID_HANDLE_VALUE) {
        std::fprintf(stderr, "Could not enumerate processes (error %lu).\n", GetLastError());
        return 2;
    }
    PROCESSENTRY32W process_entry{};
    process_entry.dwSize = sizeof(process_entry);
    DWORD pid{};
    if (Process32FirstW(processes, &process_entry)) {
        do {
            if (_wcsicmp(process_entry.szExeFile, L"Minecraft.Windows.exe") == 0 &&
                (!requested_pid||process_entry.th32ProcessID==requested_pid)) {
                pid = process_entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(processes, &process_entry));
    }
    CloseHandle(processes);
    if (!pid) {
        std::fprintf(stderr, "Minecraft.Windows.exe is not running.\n");
        return 3;
    }

    const HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!process) {
        std::fprintf(stderr, "Could not open Minecraft read-only (error %lu).\n", GetLastError());
        return 5;
    }

    std::uintptr_t base{};
    std::wstring path;
    if (!process_module(process, base, path)) {
        std::fprintf(stderr, "Could not locate the Minecraft module (error %lu).\n", GetLastError());
        CloseHandle(process);
        return 4;
    }

    std::array<unsigned char, 4096> header{};
    SIZE_T read{};
    if (!ReadProcessMemory(process, reinterpret_cast<void*>(base), header.data(), header.size(), &read) || read < 1024) {
        std::fprintf(stderr, "Could not read the Minecraft PE header (error %lu).\n", GetLastError());
        CloseHandle(process);
        return 6;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(header.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        static_cast<std::size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) > header.size()) {
        std::fprintf(stderr, "Minecraft has an invalid DOS header.\n");
        CloseHandle(process);
        return 7;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(header.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.SizeOfImage < 4096) {
        std::fprintf(stderr, "Minecraft has an invalid PE header.\n");
        CloseHandle(process);
        return 8;
    }

    const std::size_t image_size = nt->OptionalHeader.SizeOfImage;
    std::vector<unsigned char> image(image_size);
    std::vector<unsigned char> valid(image_size);
    std::size_t copied{};
    std::uintptr_t cursor = base;
    while (cursor < base + image_size) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQueryEx(process, reinterpret_cast<void*>(cursor), &info, sizeof(info))) break;
        const auto region_begin = (std::max)(cursor, reinterpret_cast<std::uintptr_t>(info.BaseAddress));
        const auto region_end = (std::min)(base + image_size,
            reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize);
        if (region_end <= region_begin) break;
        if (info.State == MEM_COMMIT && readable(info.Protect)) {
            SIZE_T region_read{};
            const std::size_t size = region_end - region_begin;
            if (ReadProcessMemory(process, reinterpret_cast<void*>(region_begin),
                                  image.data() + (region_begin - base), size, &region_read) && region_read != 0) {
                std::fill_n(valid.data() + (region_begin - base), region_read, static_cast<unsigned char>(1));
                copied += region_read;
            }
        }
        cursor = region_end;
    }

    std::wprintf(L"Minecraft process : %lu\n", pid);
    std::wprintf(L"Executable        : %ls\n", path.c_str());
    std::printf("Image base        : 0x%llX\n", static_cast<unsigned long long>(base));
    std::printf("PE timestamp      : 0x%08X\n", nt->FileHeader.TimeDateStamp);
    std::printf("PE image size     : 0x%08X\n", nt->OptionalHeader.SizeOfImage);
    std::printf("Readable bytes    : %zu / %zu\n\n", copied, image_size);
    if(dump_path){
        // Only the main executable image is captured, never arbitrary heap or
        // world memory. CREATE_NEW prevents overwriting an existing artifact.
        HANDLE file=CreateFileW(dump_path,GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        DWORD written{};
        const bool ok=file!=INVALID_HANDLE_VALUE&&WriteFile(file,image.data(),static_cast<DWORD>(image.size()),&written,nullptr)&&written==image.size();
        if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
        if(!ok){std::fprintf(stderr,"Mapped-image capture failed (error %lu).\n",GetLastError());CloseHandle(process);return 9;}
        std::printf("Mapped executable captured locally; do not upload this artifact.\n");
        CloseHandle(process);return 0;
    }

    for (const auto& source : patterns) {
        const auto pattern = parse(source);
        const auto matches = find_all(image, valid, pattern);
        std::printf("[%s] matches=%zu", pattern.name.c_str(), matches.size());
        const std::size_t shown = (std::min<std::size_t>)(matches.size(), 16);
        for (std::size_t index = 0; index < shown; ++index) {
            std::printf(" 0x%zX", matches[index]);
        }
        if (matches.size() > shown) std::printf(" ...");
        std::printf("\n");
        if (pattern.list_pointer_references && matches.size() == 1) {
            const auto references = pointer_references(image, valid, base + matches.front());
            std::printf("  absolute pointer refs=%zu", references.size());
            const std::size_t refs_shown = (std::min<std::size_t>)(references.size(), 24);
            for (std::size_t index = 0; index < refs_shown; ++index) {
                std::printf(" 0x%zX", references[index]);
            }
            if (references.size() > refs_shown) std::printf(" ...");
            std::printf("\n");
        }
    }

    CloseHandle(process);
    return 0;
}


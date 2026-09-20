#include "Commands.hpp"
#include "ChatStyle.hpp"
#include "ModuleManager.hpp"
#include "../integration/NavigationBridge.hpp"
#include "../integration/GameContext.hpp"
#include "../integration/WorldSeed.hpp"
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace utility {
namespace {
std::string lower(std::string_view value) {
    std::string result(value);
    for (auto& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return result;
}
bool tokenize(std::string_view input, std::vector<std::string>& args) {
    while (!input.empty()) {
        const auto start = input.find_first_not_of(" \t\r\n");
        if (start == input.npos) break;
        input.remove_prefix(start);
        if (input.front() == '"') {
            input.remove_prefix(1);
            const auto end = input.find('"');
            if (end == input.npos) return false;
            args.emplace_back(input.substr(0, end)); input.remove_prefix(end + 1);
            if (!input.empty() && input.find_first_of(" \t\r\n") != 0) return false;
        } else {
            const auto end = input.find_first_of(" \t\r\n");
            args.emplace_back(input.substr(0, end));
            input.remove_prefix(end == input.npos ? input.size() : end);
        }
    }
    return true;
}
unsigned key_code(std::string_view text) {
    const auto name = lower(text);
    if (name.size() == 1 && ((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= '0' && name[0] <= '9')))
        return static_cast<unsigned>(std::toupper(static_cast<unsigned char>(name[0])));
    if (name.size() > 1 && name[0] == 'f') {
        unsigned number{};
        const auto [end, error] = std::from_chars(name.data()+1, name.data()+name.size(), number);
        if (error == std::errc{} && end == name.data()+name.size() && number >= 1 && number <= 24) return VK_F1+number-1;
    }
    struct NamedKey { std::string_view name; unsigned code; };
    constexpr NamedKey keys[]{ {"space",VK_SPACE},{"shift",VK_SHIFT},{"ctrl",VK_CONTROL},{"control",VK_CONTROL},
        {"alt",VK_MENU},{"lshift",VK_LSHIFT},{"rshift",VK_RSHIFT},{"lctrl",VK_LCONTROL},{"rctrl",VK_RCONTROL},
        {"lalt",VK_LMENU},{"ralt",VK_RMENU},{"capslock",VK_CAPITAL},{"insert",VK_INSERT},{"delete",VK_DELETE},
        {"home",VK_HOME},{"end",VK_END},{"pageup",VK_PRIOR},{"pagedown",VK_NEXT},{"backspace",VK_BACK},
        {"minus",VK_OEM_MINUS},{"plus",VK_OEM_PLUS},{"period",VK_OEM_PERIOD},{"comma",VK_OEM_COMMA},
        {"slash",VK_OEM_2},{"semicolon",VK_OEM_1},{"quote",VK_OEM_7},{"backslash",VK_OEM_5},
        {"lbracket",VK_OEM_4},{"rbracket",VK_OEM_6},{"grave",VK_OEM_3} };
    for (const auto& key : keys) if (key.name == name) return key.code;
    if (name.size()==7 && name.starts_with("numpad") && name[6]>='0' && name[6]<='9') return VK_NUMPAD0+name[6]-'0';
    return 0;
}
bool matches(unsigned binding, const std::array<bool,256>& pressed) {
    if (binding == VK_SHIFT) return pressed[VK_SHIFT] || pressed[VK_LSHIFT] || pressed[VK_RSHIFT];
    if (binding == VK_CONTROL) return pressed[VK_CONTROL] || pressed[VK_LCONTROL] || pressed[VK_RCONTROL];
    if (binding == VK_MENU) return pressed[VK_MENU] || pressed[VK_LMENU] || pressed[VK_RMENU];
    return pressed[binding];
}
std::string key_name(unsigned code) {
    if (code>='A'&&code<='Z') return std::string(1,static_cast<char>(code));
    if (code>='0'&&code<='9') return std::string(1,static_cast<char>(code));
    if (code>=VK_F1&&code<=VK_F24) return "F"+std::to_string(code-VK_F1+1);
    struct NamedKey { unsigned code; std::string_view name; };
    constexpr NamedKey keys[]{
        {VK_SPACE,"Space"},{VK_SHIFT,"Shift"},{VK_CONTROL,"Ctrl"},{VK_MENU,"Alt"},
        {VK_LSHIFT,"LShift"},{VK_RSHIFT,"RShift"},{VK_LCONTROL,"LCtrl"},{VK_RCONTROL,"RCtrl"},
        {VK_LMENU,"LAlt"},{VK_RMENU,"RAlt"},{VK_CAPITAL,"CapsLock"},{VK_INSERT,"Insert"},
        {VK_DELETE,"Delete"},{VK_HOME,"Home"},{VK_END,"End"},{VK_PRIOR,"PageUp"},
        {VK_NEXT,"PageDown"},{VK_BACK,"Backspace"},{VK_OEM_MINUS,"Minus"},{VK_OEM_PLUS,"Plus"},
        {VK_OEM_PERIOD,"Period"},{VK_OEM_COMMA,"Comma"},{VK_OEM_2,"Slash"},{VK_OEM_1,"Semicolon"},
        {VK_OEM_7,"Quote"},{VK_OEM_5,"Backslash"},{VK_OEM_4,"LBracket"},{VK_OEM_6,"RBracket"},
        {VK_OEM_3,"Grave"}
    };
    for (const auto& key:keys) if(key.code==code) return std::string(key.name);
    if(code>=VK_NUMPAD0&&code<=VK_NUMPAD9) return "Numpad"+std::to_string(code-VK_NUMPAD0);
    return "VK "+std::to_string(code);
}
bool system_clipboard_write(std::string_view text) {
    if(text.empty()) return false;
    const int chars=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
    if(chars<=0) return false;
    const auto bytes=(static_cast<std::size_t>(chars)+1)*sizeof(wchar_t);
    HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,bytes);
    if(!memory) return false;
    auto* data=static_cast<wchar_t*>(GlobalLock(memory));
    if(!data){GlobalFree(memory);return false;}
    const int written=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),data,chars);
    if(written!=chars){GlobalUnlock(memory);GlobalFree(memory);return false;}
    data[chars]=L'\0';
    GlobalUnlock(memory);
    bool opened=false;
    for(int attempt=0;attempt<5 && !opened;++attempt) {
        opened=OpenClipboard(nullptr)!=FALSE;
        if(!opened) Sleep(5);
    }
    if(!opened){GlobalFree(memory);return false;}
    bool ok=false;
    if(EmptyClipboard()) ok=SetClipboardData(CF_UNICODETEXT,memory)!=nullptr;
    CloseClipboard();
    if(!ok) GlobalFree(memory);
    return ok;
}
bool current_coordinates_text(std::string& text) {
    void* player=integration::current_player();
    if(!integration::readable_game_memory(player,0x220)) return false;
    void* state{};
    std::memcpy(&state,static_cast<std::byte*>(player)+0x218,sizeof(state));
    if(!integration::readable_game_memory(state,sizeof(float)*3)) return false;
    float xyz[3]{};
    std::memcpy(xyz,state,sizeof(xyz));
    if(!std::isfinite(xyz[0])||!std::isfinite(xyz[1])||!std::isfinite(xyz[2])) return false;
    char buffer[96]{};
    const int size=std::snprintf(buffer,sizeof(buffer),"%.2f %.2f %.2f",
        static_cast<double>(xyz[0]),static_cast<double>(xyz[1]),static_cast<double>(xyz[2]));
    if(size<=0||static_cast<std::size_t>(size)>=sizeof(buffer)) return false;
    text.assign(buffer,static_cast<std::size_t>(size));
    return true;
}
constexpr auto usage = "Usage: .keybind <module> <key> <toggle|keyhold> (quote names containing spaces).";
}
bool Commands::execute(std::string_view text, ModuleManager& modules, std::vector<std::string>& replies) {
    if (text.empty() || text.front() != Commands::prefix) return false;
    // Apply a consistent error/usage color to every early-return response.
    struct StyleReplies {
        std::vector<std::string>& lines;
        std::size_t first;
        ~StyleReplies() {
            for (auto i=first; i<lines.size(); ++i) {
                auto& line=lines[i];
                if (!line.starts_with("\xC2\xA7"))
                    line.insert(0,line.starts_with("Usage:") ? chat_style::yellow : chat_style::red);
                line += chat_style::reset;
            }
        }
    } style{replies,replies.size()};
    std::vector<std::string> args;
    if (!tokenize(text.substr(1), args)) { replies.emplace_back("Unclosed or misplaced quote. Quote the complete module name; type .help for usage."); return true; }
    const auto name = args.empty() ? std::string{} : lower(args[0]);
    if (name == "help") {
        if (args.size() != 1) replies.emplace_back("Usage: .help (no arguments).");
        else {
            replies.emplace_back(std::string(chat_style::aqua)+".help"+chat_style::gray+" - Show commands.");
            replies.emplace_back(std::string(chat_style::aqua)+".loki"+chat_style::gray+" - Show Loki version and confirmed features.");
            replies.emplace_back(std::string(chat_style::aqua)+".seed"+chat_style::gray+" - Show the current world/server seed supplied to this client.");
            replies.emplace_back(std::string(chat_style::aqua)+".locate"+chat_style::gray+" - List visible players and their current coordinates.");
            replies.emplace_back(std::string(chat_style::aqua)+".keybind "+chat_style::white+"<module> <key> <toggle|keyhold>"+chat_style::gray+" - Bind a module; quote spaced names.");
            replies.emplace_back(std::string(chat_style::aqua)+".unbind "+chat_style::white+"<module>"+chat_style::gray+" - Remove all key bindings for a module.");
            replies.emplace_back(std::string(chat_style::aqua)+".binds"+chat_style::gray+" - List current Loki key bindings.");
            replies.emplace_back(std::string(chat_style::aqua)+".copy "+chat_style::white+"<seed|coords|binds>"+chat_style::gray+" - Copy Loki info to the Windows clipboard.");
            replies.emplace_back(std::string(chat_style::aqua)+".eject"+chat_style::gray+" - Disable the mod for this session.");
            for (const auto& module : modules.modules())
                for (const auto& line : module->command_help())
                    replies.emplace_back(std::string(chat_style::aqua)+line);
        }
        return true;
    }
    if (name == "loki") {
        if (args.size()!=1) { replies.emplace_back("Usage: .loki (no arguments)."); return true; }
        replies.emplace_back(std::string(chat_style::green)+"Loki"+chat_style::gray+" | Minecraft 26.50");
        replies.emplace_back(std::string(chat_style::gray)+"Confirmed: "+chat_style::aqua+
            "Reach, Block Reach, X-Ray, Fullbright, ESP, ChestESP, Auto Leave, Auto Bridge");
        return true;
    }
    if (name == "seed") {
        if (args.size()!=1) { replies.emplace_back("Usage: .seed (no arguments)."); return true; }
        std::int64_t seed{};
        if (!integration::current_world_seed(seed)) {
            replies.emplace_back("World seed is not available yet.");
            return true;
        }
        replies.emplace_back(std::string(chat_style::green)+"World Seed: "+chat_style::white+std::to_string(seed));
        return true;
    }
    if (name == "locate") {
        if(args.size()!=1) { replies.emplace_back("Usage: .locate (no arguments)."); return true; }
        std::function<integration::PlayerLocatorResult()> locator;
        {
            std::lock_guard lock(mutex_);
            locator=player_locator_;
        }
        if(!locator) { replies.emplace_back("Player locator is unavailable in this session."); return true; }
        const auto result=locator();
        if(result.status==integration::PlayerLocatorStatus::unavailable) {
            replies.emplace_back("Player positions are not available yet.");
            return true;
        }
        if(result.status==integration::PlayerLocatorStatus::no_players||result.players.empty()) {
            replies.emplace_back(std::string(chat_style::yellow)+"No other visible players are in this dimension.");
            return true;
        }
        replies.emplace_back(std::string(chat_style::aqua)+"Visible players (nearest first):");
        constexpr std::size_t maximum_lines=8;
        const auto count=(std::min)(result.players.size(),maximum_lines);
        for(std::size_t index=0;index<count;++index) {
            const auto& player=result.players[index];
            char line[160]{};
            std::snprintf(line,sizeof(line),"Player #%u: %.1f %.1f %.1f (%.1fm)",
                player.runtime_id&0x3FFFFU,static_cast<double>(player.x),static_cast<double>(player.y),
                static_cast<double>(player.z),static_cast<double>(player.distance));
            replies.emplace_back(std::string(chat_style::white)+line);
        }
        if(result.players.size()>maximum_lines)
            replies.emplace_back(std::string(chat_style::gray)+"...and "+
                std::to_string(result.players.size()-maximum_lines)+" more visible players.");
        return true;
    }
    if (name == "binds") {
        if (args.size()!=1) { replies.emplace_back("Usage: .binds (no arguments)."); return true; }
        std::lock_guard lock(mutex_);
        if(bindings_.empty()) {
            replies.emplace_back(std::string(chat_style::yellow)+"No key bindings.");
            return true;
        }
        replies.emplace_back(std::string(chat_style::aqua)+"Key bindings:");
        for(const auto& binding:bindings_) {
            replies.emplace_back(std::string(chat_style::white)+std::string(binding.module->name())+
                chat_style::gray+" -> "+chat_style::aqua+key_name(binding.key)+chat_style::gray+
                " ("+(binding.hold?"keyhold":"toggle")+")");
        }
        return true;
    }
    if (name == "copy") {
        if(args.size()!=2) { replies.emplace_back("Usage: .copy <seed|coords|binds>."); return true; }
        const auto what=lower(args[1]);
        std::string text;
        if(what=="seed") {
            std::int64_t seed{};
            if(!integration::current_world_seed(seed)) {
                replies.emplace_back("World seed is not available yet.");
                return true;
            }
            text=std::to_string(seed);
        } else if(what=="coords") {
            if(!current_coordinates_text(text)) {
                replies.emplace_back("Coordinates are not available yet.");
                return true;
            }
        } else if(what=="binds") {
            std::lock_guard lock(mutex_);
            if(bindings_.empty()) {
                replies.emplace_back("No key bindings to copy.");
                return true;
            }
            for(std::size_t i=0;i<bindings_.size();++i) {
                const auto& binding=bindings_[i];
                if(i) text+="\r\n";
                text+=std::string(binding.module->name())+" -> "+key_name(binding.key)+
                    " ("+(binding.hold?"keyhold":"toggle")+")";
            }
        } else {
            replies.emplace_back("Usage: .copy <seed|coords|binds>.");
            return true;
        }
        std::function<bool(std::string_view)> writer;
        {
            std::lock_guard lock(mutex_);
            writer=clipboard_writer_;
        }
        const bool copied=writer?writer(text):system_clipboard_write(text);
        if(!copied) {
            replies.emplace_back("Could not access the Windows clipboard. Try again.");
            return true;
        }
        replies.emplace_back(std::string(chat_style::green)+"Copied "+chat_style::aqua+what+chat_style::green+" to clipboard.");
        return true;
    }
    if (name == "eject") {
        if (args.size()!=1) { replies.emplace_back("Usage: .eject (no arguments)."); return true; }
        std::lock_guard lock(mutex_);
        if (eject_pending_) replies.emplace_back(std::string(chat_style::yellow)+"Ejection is already in progress.");
        else if (!eject_handler_ || !eject_handler_()) replies.emplace_back("Could not start ejection. Try again.");
        else {
            eject_pending_=true;
            replies.emplace_back(std::string(chat_style::green)+"Ejecting utility mod...");
        }
        return true;
    }
    if (name != "keybind" && name != "unbind") {
        for (const auto& candidate : modules.modules())
            if (candidate->handle_command(args,replies)) return true;
        replies.emplace_back("Unrecognized utility command. Type .help for available commands."); return true;
    }
    if (args.size() != (name == "unbind" ? 2U : 4U)) {
        replies.emplace_back(name == "unbind" ? "Usage: .unbind <module> (quote names containing spaces)." : usage);
        return true;
    }
    Module* module{};
    for (const auto& candidate : modules.modules()) if (lower(candidate->name()) == lower(args[1])) { module=candidate.get(); break; }
    if (!module) { replies.emplace_back("Unknown module: " + args[1] + ". Use the name shown in the module menu."); return true; }
    if (name == "unbind") {
        std::lock_guard lock(mutex_);
        // Erase every binding for this module, preserving other modules on shared keys.
        // Only an active keyhold owns the module's enabled state; toggles keep theirs.
        const auto removed = std::erase_if(bindings_, [module](const Binding& binding) {
            if (binding.module != module) return false;
            if (binding.hold && binding.active) module->set_enabled(false);
            return true;
        });
        if (removed) replies.emplace_back(std::string(chat_style::green)+"Unbound "+chat_style::aqua+std::string(module->name())+chat_style::green+" from all keys.");
        else replies.emplace_back(std::string(chat_style::yellow)+std::string(module->name())+" has no key bindings.");
        return true;
    }
    const unsigned code = key_code(args[2]);
    if (!code) { replies.emplace_back("Unknown or reserved key: " + args[2] + ". Use A-Z, 0-9, F1-F24 or a named key (e.g. space, lshift)."); return true; }
    const auto mode = lower(args[3]);
    if (mode != "toggle" && mode != "keyhold") { replies.emplace_back(usage); return true; }
    if (!module->usable()) { replies.emplace_back("Module unavailable in this session: " + std::string(module->name())); return true; }
    std::lock_guard lock(mutex_);
    auto found = std::find_if(bindings_.begin(), bindings_.end(), [module](const Binding& b){return b.module==module;});
    if (found != bindings_.end()) {
        if (found->hold && found->active) module->set_enabled(false);
        *found = {module,code,mode=="keyhold"};
    } else bindings_.push_back({module,code,mode=="keyhold"});
    if (mode=="keyhold") module->set_enabled(false);
    replies.emplace_back(std::string(chat_style::green)+"Bound "+chat_style::aqua+std::string(module->name())+chat_style::green+" to "+chat_style::white+args[2]+chat_style::gray+" ("+mode+").");
    return true;
}
void Commands::set_eject_handler(std::function<bool()> handler) {
    std::lock_guard lock(mutex_); eject_handler_=std::move(handler); eject_pending_=false;
}
void Commands::set_clipboard_writer(std::function<bool(std::string_view)> writer) {
    std::lock_guard lock(mutex_);
    clipboard_writer_=std::move(writer);
}
void Commands::set_player_locator(std::function<integration::PlayerLocatorResult()> locator) {
    std::lock_guard lock(mutex_);
    player_locator_=std::move(locator);
}
void Commands::key(unsigned code, bool down, bool gameplay) {
    if (code >= pressed_.size()) return;
    std::lock_guard lock(mutex_);
    const auto before = pressed_;
    pressed_[code] = down;
    for (auto& b : bindings_) {
        const bool held = matches(b.key, pressed_);
        if (b.hold && b.active && (!held || !gameplay)) { b.module->set_enabled(false); b.active=false; }
        if (gameplay && held && !matches(b.key,before)) {
            if (b.hold) { b.module->set_enabled(true); b.active=true; }
            else b.module->set_enabled(!b.module->enabled());
        }
    }
}
void Commands::suspend() {
    integration::navigation_suspend();
    std::lock_guard lock(mutex_);
    // Preserve physical edge state: resuming while a key is held must not activate it.
    for (auto& b : bindings_) if (b.hold && b.active) { b.module->set_enabled(false); b.active=false; }
}
void Commands::clear() {
    suspend();
    std::lock_guard lock(mutex_); bindings_.clear(); pressed_.fill(false); eject_handler_={}; clipboard_writer_={};
    player_locator_={}; eject_pending_=false;
}
}

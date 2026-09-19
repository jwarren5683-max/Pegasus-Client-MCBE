#pragma once
#include <array>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace utility {
class Module;
class ModuleManager;
class Commands final {
public:
    static constexpr char prefix = '.';
    // true always means consume locally, including invalid local commands.
    bool execute(std::string_view text, ModuleManager& modules, std::vector<std::string>& replies);
    // The handler only schedules shutdown; it must not destroy modules inline.
    void set_eject_handler(std::function<bool()> handler);
    void set_clipboard_writer(std::function<bool(std::string_view)> writer);
    void key(unsigned key, bool down, bool gameplay);
    void suspend();
    void clear();
private:
    struct Binding { Module* module; unsigned key; bool hold; bool active{}; };
    std::mutex mutex_;
    std::function<bool()> eject_handler_;
    std::function<bool(std::string_view)> clipboard_writer_;
    bool eject_pending_{};
    std::vector<Binding> bindings_;
    std::array<bool, 256> pressed_{};
};
}

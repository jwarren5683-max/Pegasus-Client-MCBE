#pragma once

#include <cstdint>

// AutoBridge is deliberately an input-assist policy rather than a native
// placement call.  The current Bedrock 1.26.5101.0 profile has no verified
// inventory, hit-result, or placement interface, so the policy only emits a
// normal right-click while the user holds an explicit modifier and looks down.
// Every gate is fail-closed and the cadence is deterministic for unit tests.
namespace utility::modules::auto_bridge {

inline constexpr std::uint64_t default_interval_ms = 110;

struct Input {
    bool enabled{};
    bool local_world{};
    bool foreground{};
    bool alive{};
    bool modifier{};       // dedicated hold key (V)
    bool forward{};        // W, never synthesized by the module
    bool jump{};           // Space, keeps the user in the normal jump loop
    bool looking_down{};   // pitch guard; prevents clicks at menus/sky
    bool selected_block{}; // supplied by the caller when a block is selected
};

struct Decision { bool place{}; };

class Cadence final {
public:
    explicit Cadence(std::uint64_t interval_ms = default_interval_ms) noexcept
        : interval_ms_(interval_ms ? interval_ms : default_interval_ms) {}

    Decision update(std::uint64_t now, const Input& input) noexcept {
        const bool allowed = input.enabled && input.local_world && input.foreground &&
            input.alive && input.modifier && input.forward && input.jump &&
            input.looking_down && input.selected_block;
        if (!allowed) {
            armed_ = false;
            last_ = 0;
            return {};
        }
        if (!armed_) {
            armed_ = true;
            last_ = now;
            return {true};
        }
        if (now < last_ || now - last_ >= interval_ms_) {
            last_ = now;
            return {true};
        }
        return {};
    }

    void reset() noexcept { armed_ = false; last_ = 0; }
    [[nodiscard]] bool armed() const noexcept { return armed_; }

private:
    std::uint64_t interval_ms_{};
    std::uint64_t last_{};
    bool armed_{};
};

} // namespace utility::modules::auto_bridge


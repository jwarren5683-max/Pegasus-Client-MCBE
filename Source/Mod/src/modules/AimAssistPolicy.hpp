#pragma once

#include <algorithm>
#include <cmath>

// Aim Assist is intentionally a small input-level correction policy. The
// caller supplies a screen-space error for a validated player box; this policy
// enforces an explicit hold gate, a finite field of view and bounded smoothing.
namespace utility::modules::aim_assist {

inline constexpr float default_fov_pixels = 180.0F;
inline constexpr float default_gain = 0.28F;
inline constexpr float default_max_step_pixels = 10.0F;

struct Input {
    bool enabled{};
    bool local_world{};
    bool foreground{};
    bool alive{};
    bool modifier{}; // keyboard C or controller LT
};

struct Target {
    bool valid{};
    bool player{};
    float error_x{}; // target minus screen centre, in pixels
    float error_y{};
    float depth{};
};

struct Correction { bool adjust{}; int dx{}; int dy{}; };

inline Correction correct(const Input& input, const Target& target,
    float fov_pixels = default_fov_pixels, float gain = default_gain,
    float max_step_pixels = default_max_step_pixels) noexcept {
    if(!input.enabled||!input.local_world||!input.foreground||!input.alive||!input.modifier||
       !target.valid||!target.player||!std::isfinite(target.error_x)||!std::isfinite(target.error_y)||
       !std::isfinite(target.depth)||target.depth<=0||!std::isfinite(fov_pixels)||fov_pixels<=0||
       !std::isfinite(gain)||gain<=0||!std::isfinite(max_step_pixels)||max_step_pixels<1)return {};
    const auto radius=std::hypot(target.error_x,target.error_y);
    if(!std::isfinite(radius)||radius>fov_pixels)return {};
    const auto bounded=[&](float error) {
        return static_cast<int>(std::clamp(std::lround(error*gain),
            -static_cast<long>(std::floor(max_step_pixels)),static_cast<long>(std::floor(max_step_pixels))));
    };
    const int dx=bounded(target.error_x),dy=bounded(target.error_y);
    return {dx!=0||dy!=0,dx,dy};
}

} // namespace utility::modules::aim_assist


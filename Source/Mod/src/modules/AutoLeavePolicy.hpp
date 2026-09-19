#pragma once

#include <algorithm>
#include <cmath>

namespace utility::modules::auto_leave {

inline constexpr float minimum_hearts = 0.5F;
inline constexpr float maximum_hearts = 10.0F;
inline constexpr float default_hearts = 4.0F;

[[nodiscard]] inline float clamp_hearts(float hearts) noexcept {
    if (!std::isfinite(hearts)) return default_hearts;
    return std::clamp(std::round(hearts * 2.0F) * 0.5F, minimum_hearts, maximum_hearts);
}

class Gate final {
public:
    // Two consecutive low samples reject a single torn/native-transition read while
    // adding no more than one normal player tick of delay.
    [[nodiscard]] bool update(float health_points, float threshold_hearts, bool enabled) noexcept {
        if (!enabled || !std::isfinite(health_points) || health_points < 0.0F || health_points > 20.0F) {
            below_samples_ = 0;
            return false;
        }
        if (fired_) return false;
        const float threshold_points = clamp_hearts(threshold_hearts) * 2.0F;
        if (health_points > threshold_points) {
            below_samples_ = 0;
            return false;
        }
        if (++below_samples_ < 2) return false;
        fired_ = true;
        return true;
    }

    void reset() noexcept {
        below_samples_ = 0;
        fired_ = false;
    }

    [[nodiscard]] bool fired() const noexcept { return fired_; }

private:
    unsigned below_samples_{};
    bool fired_{};
};

} // namespace utility::modules::auto_leave


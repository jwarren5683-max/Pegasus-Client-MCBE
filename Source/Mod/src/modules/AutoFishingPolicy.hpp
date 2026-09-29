#pragma once

#include <cmath>
#include <cstdint>

namespace utility::modules::auto_fishing {

inline constexpr std::uint64_t minimum_hook_age_ms = 1200;
inline constexpr std::uint64_t maximum_sample_gap_ms = 250;
inline constexpr std::uint64_t recast_delay_ms = 350;
inline constexpr std::uint64_t cast_timeout_ms = 3500;
inline constexpr float bite_drop_blocks = 0.08F;

struct Input {
    bool enabled{};
    bool local_world{};
    bool foreground{};
    bool alive{};
    bool sample_valid{};
    bool hook_present{};
    std::uint64_t hook_id{};
    float hook_y{};
};

struct Decision {
    bool reel{};
    bool cast{};
};

// Auto Fishing emits ordinary right-click pulses. It casts once when enabled,
// waits for a validated nearby fishing hook, reels when that hook makes a
// sudden downward bite movement, then recasts after the hook disappears.
// Failed casts time out instead of producing repeated clicks.
class Controller final {
public:
    Decision update(std::uint64_t now, const Input& input) noexcept {
        if (!input.enabled || !input.local_world || !input.foreground || !input.alive) {
            reset();
            return {};
        }
        if (!input.sample_valid) return {};

        if (phase_ == Phase::waiting_for_hook && now >= action_time_ &&
            now - action_time_ > cast_timeout_ms) {
            phase_ = Phase::stopped;
        }

        if (!input.hook_present || !input.hook_id || !std::isfinite(input.hook_y)) {
            clear_hook();
            if (phase_ == Phase::idle) {
                phase_ = Phase::waiting_for_hook;
                action_time_ = now;
                return {.cast = true};
            }
            if (phase_ == Phase::waiting_for_disappear) {
                if (!missing_since_) missing_since_ = now;
                if (now >= missing_since_ && now - missing_since_ >= recast_delay_ms) {
                    phase_ = Phase::waiting_for_hook;
                    action_time_ = now;
                    missing_since_ = 0;
                    return {.cast = true};
                }
            }
            return {};
        }

        missing_since_ = 0;
        if (phase_ == Phase::waiting_for_disappear) return {};
        if (phase_ == Phase::idle || phase_ == Phase::waiting_for_hook ||
            phase_ == Phase::stopped || hook_id_ != input.hook_id) {
            phase_ = Phase::watching;
            hook_id_ = input.hook_id;
            first_seen_ = last_sample_ = now;
            last_y_ = input.hook_y;
            return {};
        }

        if (now < last_sample_ || now < first_seen_) {
            reset();
            return {};
        }
        const auto gap = now - last_sample_;
        const auto age = now - first_seen_;
        const float drop = input.hook_y - last_y_;
        last_sample_ = now;
        last_y_ = input.hook_y;
        if (phase_ == Phase::watching && age >= minimum_hook_age_ms &&
            gap <= maximum_sample_gap_ms && drop <= -bite_drop_blocks) {
            phase_ = Phase::waiting_for_disappear;
            action_time_ = now;
            return {.reel = true};
        }
        return {};
    }

    void reset() noexcept {
        phase_ = Phase::idle;
        clear_hook();
        missing_since_ = action_time_ = 0;
    }

private:
    enum class Phase { idle, waiting_for_hook, watching, waiting_for_disappear, stopped };

    void clear_hook() noexcept {
        hook_id_ = 0;
        first_seen_ = last_sample_ = 0;
        last_y_ = 0.0F;
    }

    Phase phase_{Phase::idle};
    std::uint64_t hook_id_{};
    std::uint64_t first_seen_{};
    std::uint64_t last_sample_{};
    std::uint64_t missing_since_{};
    std::uint64_t action_time_{};
    float last_y_{};
};

} // namespace utility::modules::auto_fishing

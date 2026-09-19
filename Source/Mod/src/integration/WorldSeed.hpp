#pragma once

#include <atomic>
#include <cstdint>

namespace utility::integration {

namespace world_seed_detail {
inline std::atomic<std::uint64_t> value{};
inline std::atomic_bool valid{};
}

inline void reset_world_seed() noexcept {
    world_seed_detail::valid.store(false, std::memory_order_release);
}

inline void publish_world_seed(std::uint64_t seed) noexcept {
    world_seed_detail::value.store(seed, std::memory_order_relaxed);
    world_seed_detail::valid.store(true, std::memory_order_release);
}

[[nodiscard]] inline bool current_world_seed(std::int64_t& seed) noexcept {
    if (!world_seed_detail::valid.load(std::memory_order_acquire)) return false;
    seed = static_cast<std::int64_t>(world_seed_detail::value.load(std::memory_order_relaxed));
    return true;
}

} // namespace utility::integration

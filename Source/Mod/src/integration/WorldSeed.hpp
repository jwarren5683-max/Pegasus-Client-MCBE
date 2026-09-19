#pragma once

#include <cstdint>
#include <mutex>

namespace utility::integration {

namespace world_seed_detail {
inline std::mutex mutex;
inline std::uint64_t value{};
inline bool valid{};
}

inline void reset_world_seed() noexcept {
    std::lock_guard lock(world_seed_detail::mutex);
    world_seed_detail::valid=false;
    world_seed_detail::value=0;
}

inline void publish_world_seed(std::uint64_t seed) noexcept {
    std::lock_guard lock(world_seed_detail::mutex);
    world_seed_detail::value=seed;
    world_seed_detail::valid=true;
}

[[nodiscard]] inline bool current_world_seed(std::int64_t& seed) noexcept {
    std::lock_guard lock(world_seed_detail::mutex);
    if(!world_seed_detail::valid) return false;
    seed=static_cast<std::int64_t>(world_seed_detail::value);
    return true;
}

} // namespace utility::integration

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace utility::integration {

struct PlayerLocation {
    std::uint32_t runtime_id{};
    float x{},y{},z{},distance{};
    std::string name;
};

enum class PlayerLocatorStatus { unavailable, no_players, ready };

struct PlayerLocatorResult {
    PlayerLocatorStatus status{PlayerLocatorStatus::unavailable};
    std::vector<PlayerLocation> players;
};

} // namespace utility::integration

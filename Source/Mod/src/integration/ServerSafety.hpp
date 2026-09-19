#pragma once

#include <Windows.h>

#include <atomic>

namespace utility::integration::server_safety {

// Remote servers are authoritative and can prohibit gameplay-altering
// modules. Detect an integrated server by observing its local-player replica;
// if client ticks continue without that replica, fail closed as remote.
inline std::atomic<ULONGLONG> client_tick_time{};
inline std::atomic<ULONGLONG> integrated_server_tick_time{};
constexpr ULONGLONG active_window_ms = 1500;

inline void observe_client_tick(ULONGLONG now = GetTickCount64()) noexcept {
    client_tick_time.store(now, std::memory_order_release);
}

inline void observe_integrated_server_tick(ULONGLONG now = GetTickCount64()) noexcept {
    integrated_server_tick_time.store(now, std::memory_order_release);
}

[[nodiscard]] inline bool remote_session(ULONGLONG now = GetTickCount64()) noexcept {
    const auto client = client_tick_time.load(std::memory_order_acquire);
    if (!client || now < client || now - client > active_window_ms) return false;
    const auto local_server = integrated_server_tick_time.load(std::memory_order_acquire);
    return !local_server || now < local_server || now - local_server > active_window_ms;
}

[[nodiscard]] inline bool local_world(ULONGLONG now = GetTickCount64()) noexcept {
    const auto client = client_tick_time.load(std::memory_order_acquire);
    const auto local_server = integrated_server_tick_time.load(std::memory_order_acquire);
    return client && local_server && now >= client && now >= local_server &&
        now - client <= active_window_ms && now - local_server <= active_window_ms;
}

inline void reset() noexcept {
    client_tick_time.store(0, std::memory_order_release);
    integrated_server_tick_time.store(0, std::memory_order_release);
}

} // namespace utility::integration::server_safety

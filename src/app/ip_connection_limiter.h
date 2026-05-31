// Per-IP connection admission limiter.
//
// This small helper keeps one client IP from consuming disproportionate queue
// and worker capacity by capping the number of accepted in-flight connections
// attributed to that peer.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace app {

class ip_connection_limiter
{
public:
    explicit ip_connection_limiter(std::size_t per_ip_limit);

    [[nodiscard]] bool try_acquire(std::string_view peer_address);
    void release(std::string_view peer_address) noexcept;
    [[nodiscard]] std::size_t per_ip_limit() const noexcept;

private:
    std::size_t _per_ip_limit = 0;
    mutable std::mutex _mutex;
    std::unordered_map<std::string, std::size_t> _inflight_by_ip;
};

} // namespace app

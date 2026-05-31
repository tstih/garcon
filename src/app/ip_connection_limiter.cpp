// Implementation of the per-IP connection admission limiter.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "app/ip_connection_limiter.h"

#include <stdexcept>

namespace app {

ip_connection_limiter::ip_connection_limiter(std::size_t per_ip_limit)
    : _per_ip_limit(per_ip_limit)
{
    if (_per_ip_limit == 0)
        throw std::invalid_argument("ip_connection_limiter limit must be greater than zero");
}

bool ip_connection_limiter::try_acquire(std::string_view peer_address)
{
    if (peer_address.empty())
        return true;

    std::lock_guard lock(_mutex);
    auto& count = _inflight_by_ip[std::string(peer_address)];
    if (count >= _per_ip_limit)
        return false;

    ++count;
    return true;
}

void ip_connection_limiter::release(std::string_view peer_address) noexcept
{
    if (peer_address.empty())
        return;

    std::lock_guard lock(_mutex);
    const auto it = _inflight_by_ip.find(std::string(peer_address));
    if (it == _inflight_by_ip.end())
        return;

    if (it->second > 1) {
        --it->second;
        return;
    }

    _inflight_by_ip.erase(it);
}

std::size_t ip_connection_limiter::per_ip_limit() const noexcept
{
    return _per_ip_limit;
}

} // namespace app

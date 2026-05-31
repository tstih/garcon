// Transport-neutral byte stream abstraction.
//
// This file defines the small runtime interface that the HTTP layer uses to
// exchange bytes with a client connection. Concrete implementations may use
// plain TCP or TLS, but the framing and request parsing code only depends on
// this contract.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "net/io.h"

#include <chrono>
#include <cstddef>
#include <span>
#include <string_view>

namespace net {

class stream
{
public:
    virtual ~stream() = default;

    virtual bool is_valid() const = 0;
    virtual void close() = 0;

    virtual bool set_receive_timeout(std::chrono::milliseconds timeout) = 0;
    virtual bool set_send_timeout(std::chrono::milliseconds timeout) = 0;

    // Performs any connection setup required before application I/O begins.
    // Plain TCP streams return success immediately; TLS streams perform the
    // server-side handshake here.
    virtual io_status handshake() = 0;

    [[nodiscard]] virtual read_result recv_some(std::span<std::byte> out) = 0;
    [[nodiscard]] virtual io_status send_all(std::span<const std::byte> data) = 0;

    [[nodiscard]] io_status send_all(std::string_view s)
    {
        const auto* p = reinterpret_cast<const std::byte*>(s.data());
        return send_all(std::span<const std::byte>(p, s.size()));
    }
};

} // namespace net

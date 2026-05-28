// Plain TCP stream adapter.
//
// This file defines the net::plain_stream class, which adapts the existing
// net::socket type to the transport-neutral net::stream interface used by the
// HTTP server pipeline.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "net/socket.h"
#include "net/stream.h"

namespace net {

class plain_stream final : public stream
{
public:
    explicit plain_stream(socket socket);

    bool is_valid() const override;
    void close() override;

    bool set_receive_timeout(std::chrono::milliseconds timeout) override;
    bool set_send_timeout(std::chrono::milliseconds timeout) override;

    io_status handshake() override;
    read_result recv_some(std::span<std::byte> out) override;
    io_status send_all(std::span<const std::byte> data) override;

private:
    socket _socket;
};

} // namespace net

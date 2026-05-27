// TLS stream adapter.
//
// This file defines the tls::stream class, which adapts an accepted TCP
// socket to the transport-neutral net::stream interface using OpenSSL.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "net/socket.h"
#include "net/stream.h"
#include "tls/context.h"

#include <memory>

#include <openssl/ssl.h>

namespace tls {

class stream final : public net::stream
{
public:
    stream(const context& tls_context, net::socket socket);

    bool valid() const override;
    void close() override;

    bool set_receive_timeout(std::chrono::milliseconds timeout) override;
    bool set_send_timeout(std::chrono::milliseconds timeout) override;

    net::io_status handshake() override;
    net::read_result recv_some(std::span<std::byte> out) override;
    net::io_status send_all(std::span<const std::byte> data) override;

private:
    using ssl_ptr = std::unique_ptr<SSL, decltype(&::SSL_free)>;

    net::socket _socket;
    ssl_ptr _ssl;
};

} // namespace tls

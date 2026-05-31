// Per-connection runtime for Garçon request handling.
//
// This class owns the lifecycle of one accepted TCP/TLS connection, including
// handshake, request framing, keep-alive policy, pipeline dispatch, response
// writes, and per-request access logging.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "app/access_log.h"
#include "app/request_handler.h"
#include "app/runtime_events.h"
#include "app/stream_factory.h"
#include "config.h"
#include "server_config.h"

#include <chrono>

namespace net {
class stream;
}

namespace app {

class connection_handler
{
public:
    connection_handler(const request_handler& handler,
                       const stream_factory& stream_factory,
                       runtime_events& events,
                       access_log& access_log,
                       const server_config& config);

    void handle_connection(net::socket client) const;

private:
    static constexpr auto io_timeout = garcon::config::socket_io_timeout;
    static constexpr auto keep_alive_idle_timeout =
        garcon::config::keep_alive_idle_timeout;

    const request_handler& _handler;
    const stream_factory& _stream_factory;
    runtime_events& _events;
    access_log& _access_log;
    bool _https_enabled = false;

    bool prepare_stream(net::stream& client) const;
    bool send_response(net::stream& client,
                       std::string_view wire_response) const;
    void log_access(std::string_view client_ip,
                    const http::request* request,
                    const http::response& response,
                    std::size_t wire_bytes,
                    std::chrono::steady_clock::duration elapsed) const;
};

} // namespace app

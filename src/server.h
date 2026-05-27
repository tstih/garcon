// HTTP server orchestration.
//
// This file defines the app::server class, which coordinates the networking,
// HTTP parsing, and request handling layers. The server owns a listening
// socket, accepts incoming connections, and dispatches each connection to
// the appropriate request handler.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "app/request_handler.h"
#include "app/runtime_events.h"
#include "app/stream_factory.h"
#include "app/work_queue.h"
#include "net/listener.h"
#include "server_config.h"

#include <chrono>
#include <memory>

namespace net {
class stream;
}

namespace app {

class server
{
public:
    // Creates an HTTP or HTTPS server using the supplied runtime config.
    explicit server(server_config config);

    // Runs the server accept loop. This call blocks indefinitely.
    void run();

private:
    static constexpr auto io_timeout = std::chrono::seconds(5);

    server_config _config;
    net::listener _listener;
    std::unique_ptr<request_handler> _handler;
    std::unique_ptr<stream_factory> _stream_factory;
    std::unique_ptr<runtime_events> _events;

    // Handles a single client connection.
    void handle_connection(net::socket client);

    void handle_accept_result(const net::accept_result& accepted) const;
    void handle_queue_result(queue_push_result result) const;
    void send_response(net::stream& client, http::response response) const;
    bool prepare_stream(net::stream& client) const;
    void serve_client(net::stream& client) const;
    void accept_loop(work_queue& queue);
};

} // namespace app

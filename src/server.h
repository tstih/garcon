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

#include "app/access_log.h"
#include "app/connection_handler.h"
#include "app/ip_connection_limiter.h"
#include "app/request_handler.h"
#include "app/runtime_events.h"
#include "app/stream_factory.h"
#include "app/work_queue.h"
#include "config.h"
#include "net/listener.h"
#include "server_config.h"

#include <memory>

namespace app {

class server
{
public:
    // Creates an HTTP or HTTPS server using the supplied runtime config.
    explicit server(server_config config);

    // Runs the server accept loop. This call blocks indefinitely.
    void run();

private:
    server_config _config;
    net::listener _listener;
    std::unique_ptr<request_handler> _handler;
    std::unique_ptr<stream_factory> _stream_factory;
    std::unique_ptr<runtime_events> _events;
    std::unique_ptr<access_log> _access_log;
    std::unique_ptr<connection_handler> _connection_handler;
    ip_connection_limiter _ip_connection_limiter;

    void handle_accept_result(const net::accept_error& error) const;
    void handle_queue_result(queue_push_result result) const;
    void accept_loop(work_queue& queue);
};

} // namespace app

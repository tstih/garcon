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

#include "app/work_queue.h"
#include "app/worker_pool.h"
#include "net/listener.h"
#include "server_config.h"
#include "static_files.h"
#include "tls/context.h"

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
    static_files  _files;
    std::unique_ptr<tls::context> _tls_context;

    // Handles a single client connection.
    void handle_connection(net::socket client);

    std::unique_ptr<net::stream> create_stream(net::socket client) const;
    bool prepare_stream(net::stream& client) const;
    void serve_client(net::stream& client) const;
    void accept_loop(work_queue& queue);
};

} // namespace app

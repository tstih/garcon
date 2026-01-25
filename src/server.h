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

#include "net/listener.h"
#include "static_files.h"

#include <filesystem>

namespace app {

class server
{
public:
    // Creates an HTTP server listening on the given TCP port and serving
    // files from the specified root directory.
    server(int port, std::filesystem::path www_root);

    // Runs the server accept loop. This call blocks indefinitely.
    void run();

private:
    net::listener _listener;
    static_files  _files;

    // Handles a single client connection.
    void handle_connection(net::socket client);
};

} // namespace app

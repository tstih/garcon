// Program entry point for the Garçon HTTP server.
//
// This file defines the main entry point of the application. It parses basic
// command-line arguments, constructs the server instance, and starts the
// blocking server loop. Any fatal initialization errors are reported to
// standard error.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "server.h"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

int main(int argc, char** argv)
{
    const int port = (argc >= 2) ? std::stoi(argv[1]) : 8080;

    try {
        app::server s(port, fs::path("www"));
        s.run();
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

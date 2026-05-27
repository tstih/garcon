// Program entry point for the Garçon HTTP server.
//
// This file defines the main entry point of the application. It parses basic
// command-line arguments, constructs the server instance, and starts the
// blocking server loop. Any fatal initialization errors are reported to
// standard error.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "app/command_line.h"
#include "server.h"

#include <csignal>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv)
{
    std::signal(SIGPIPE, SIG_IGN);

    try {
        const app::command_line_parser parser(argc, argv);
        const auto result = parser.parse();
        if (result.show_help) {
            std::cout << parser.usage();
            return 0;
        }

        app::server s(*result.config);
        s.run();
    } catch (const std::exception& e) {
        const app::command_line_parser parser(argc, argv);
        std::cerr << "fatal: " << e.what() << "\n";
        std::cerr << parser.usage();
        return 1;
    }

    return 0;
}

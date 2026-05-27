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

#include <charconv>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <string_view>

namespace fs = std::filesystem;

namespace {

struct cli_options
{
    std::string bind_address = "127.0.0.1";
    std::optional<int> port;
    std::optional<std::size_t> worker_threads;
    std::optional<std::size_t> connection_queue_capacity;
    std::optional<fs::path> tls_certificate_file;
    std::optional<fs::path> tls_private_key_file;
};

void print_usage(const char* argv0)
{
    std::cout << "usage: " << argv0
              << " [port] [--bind ADDRESS] [--port PORT]"
              << " [--workers N] [--queue-capacity N]"
              << " [--tls-cert PATH --tls-key PATH]\n";
}

std::optional<int> parse_port(std::string_view value)
{
    int port = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, port);

    if (result.ec != std::errc{} || result.ptr != end)
        return std::nullopt;

    if (port <= 0 || port > 65535)
        return std::nullopt;

    return port;
}

std::optional<std::size_t> parse_count(std::string_view value)
{
    std::size_t parsed = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);

    if (result.ec != std::errc{} || result.ptr != end || parsed == 0)
        return std::nullopt;

    return parsed;
}

bool https_requested(const cli_options& opts)
{
    return opts.tls_certificate_file.has_value() ||
           opts.tls_private_key_file.has_value();
}

std::size_t default_worker_threads()
{
    const auto concurrency = std::thread::hardware_concurrency();
    return concurrency == 0 ? 4U : static_cast<std::size_t>(concurrency);
}

std::size_t default_queue_capacity(std::size_t worker_threads)
{
    return worker_threads * 64U;
}

fs::path default_www_root()
{
    const fs::path local_root("www");
    if (fs::exists(local_root))
        return local_root;

    const fs::path installed_root("/usr/share/garcon/www");
    if (fs::exists(installed_root))
        return installed_root;

    return local_root;
}

app::server_config finalize_options(const cli_options& opts)
{
    if (opts.tls_certificate_file.has_value() !=
        opts.tls_private_key_file.has_value()) {
        throw std::runtime_error("--tls-cert and --tls-key must be provided together");
    }

    app::server_config config;
    config.bind_address = opts.bind_address;
    config.port = opts.port.value_or(https_requested(opts) ? 8443 : 8080);
    config.www_root = default_www_root();
    config.worker_threads = opts.worker_threads.value_or(default_worker_threads());
    config.connection_queue_capacity =
        opts.connection_queue_capacity.value_or(default_queue_capacity(config.worker_threads));

    if (opts.tls_certificate_file) {
        config.tls = app::tls_config{
            .certificate_file = *opts.tls_certificate_file,
            .private_key_file = *opts.tls_private_key_file,
        };
    }

    return config;
}

std::optional<cli_options> parse_args(int argc, char** argv)
{
    cli_options out;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);

        if (arg == "--help") {
            print_usage(argv[0]);
            return std::nullopt;
        }

        if (arg == "--bind") {
            if (i + 1 >= argc)
                throw std::runtime_error("missing value for --bind");

            out.bind_address = argv[++i];
            continue;
        }

        if (arg == "--tls-cert") {
            if (i + 1 >= argc)
                throw std::runtime_error("missing value for --tls-cert");

            out.tls_certificate_file = fs::path(argv[++i]);
            continue;
        }

        if (arg == "--tls-key") {
            if (i + 1 >= argc)
                throw std::runtime_error("missing value for --tls-key");

            out.tls_private_key_file = fs::path(argv[++i]);
            continue;
        }

        if (arg == "--workers") {
            if (i + 1 >= argc)
                throw std::runtime_error("missing value for --workers");

            const auto count = parse_count(argv[++i]);
            if (!count)
                throw std::runtime_error("invalid worker count");

            out.worker_threads = count;
            continue;
        }

        if (arg == "--queue-capacity") {
            if (i + 1 >= argc)
                throw std::runtime_error("missing value for --queue-capacity");

            const auto count = parse_count(argv[++i]);
            if (!count)
                throw std::runtime_error("invalid queue capacity");

            out.connection_queue_capacity = count;
            continue;
        }

        if (arg == "--port") {
            if (i + 1 >= argc)
                throw std::runtime_error("missing value for --port");

            const auto port = parse_port(argv[++i]);
            if (!port)
                throw std::runtime_error("invalid port");

            out.port = port;
            continue;
        }

        if (!arg.empty() && arg.front() == '-')
            throw std::runtime_error("unknown option");

        const auto port = parse_port(arg);
        if (!port)
            throw std::runtime_error("invalid port");

        out.port = port;
    }

    return out;
}

} // namespace

int main(int argc, char** argv)
{
    std::signal(SIGPIPE, SIG_IGN);

    try {
        const auto opts = parse_args(argc, argv);
        if (!opts)
            return 0;

        app::server s(finalize_options(*opts));
        s.run();
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}

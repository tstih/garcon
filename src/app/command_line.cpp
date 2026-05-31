// Implementation of command-line parsing for Garcon.
//
// This file contains the small option parser that validates runtime flags and
// produces the server configuration used by the application entry point.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "app/command_line.h"

#include <charconv>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace app {

namespace {

struct cli_options
{
    std::string bind_address = "127.0.0.1";
    std::optional<int> port;
    std::optional<std::size_t> worker_threads;
    std::optional<std::size_t> connection_queue_capacity;
    std::optional<fs::path> module_config_dir;
    std::optional<fs::path> tls_certificate_file;
    std::optional<fs::path> tls_private_key_file;
};

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

fs::path default_module_config_dir(const fs::path& program_path)
{
    std::vector<fs::path> candidates;

    if (!program_path.empty()) {
        candidates.push_back(fs::absolute(program_path).parent_path() / "modules.d");
    }

    candidates.emplace_back("modules.d");
    candidates.emplace_back("/etc/garcon/modules.d");

    for (const auto& candidate : candidates) {
        if (fs::exists(candidate) && fs::is_directory(candidate))
            return candidate;
    }

    return candidates.empty() ? fs::path("modules.d") : candidates.front();
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

server_config finalize_options(const cli_options& opts, const fs::path& program_path)
{
    if (opts.tls_certificate_file.has_value() !=
        opts.tls_private_key_file.has_value()) {
        throw std::runtime_error("--tls-cert and --tls-key must be provided together");
    }

    const auto worker_threads = opts.worker_threads.value_or(default_worker_threads());

    return server_config{
        .bind_address = opts.bind_address,
        .port = opts.port.value_or(https_requested(opts) ? 8443 : 8080),
        .www_root = default_www_root(),
        .module_config_dir =
            opts.module_config_dir.value_or(default_module_config_dir(program_path)),
        .worker_threads = worker_threads,
        .connection_queue_capacity =
            opts.connection_queue_capacity.value_or(default_queue_capacity(worker_threads)),
        .tls = opts.tls_certificate_file
            ? std::optional<tls_config>{tls_config{
                  .certificate_file = *opts.tls_certificate_file,
                  .private_key_file = *opts.tls_private_key_file,
              }}
            : std::nullopt,
    };
}

std::string require_value(int argc,
                          char** argv,
                          int& index,
                          std::string_view option_name)
{
    if (index + 1 >= argc)
        throw std::runtime_error("missing value for " + std::string(option_name));

    return argv[++index];
}

void parse_named_option(std::string_view arg,
                        cli_options& out,
                        int argc,
                        char** argv,
                        int& index)
{
    if (arg == "--bind") {
        out.bind_address = require_value(argc, argv, index, arg);
        return;
    }

    if (arg == "--tls-cert") {
        out.tls_certificate_file = fs::path(require_value(argc, argv, index, arg));
        return;
    }

    if (arg == "--tls-key") {
        out.tls_private_key_file = fs::path(require_value(argc, argv, index, arg));
        return;
    }

    if (arg == "--modules-dir") {
        out.module_config_dir = fs::path(require_value(argc, argv, index, arg));
        return;
    }

    if (arg == "--workers") {
        const auto count = parse_count(require_value(argc, argv, index, arg));
        if (!count)
            throw std::runtime_error("invalid worker count");

        out.worker_threads = count;
        return;
    }

    if (arg == "--queue-capacity") {
        const auto count = parse_count(require_value(argc, argv, index, arg));
        if (!count)
            throw std::runtime_error("invalid queue capacity");

        out.connection_queue_capacity = count;
        return;
    }

    if (arg == "--port") {
        const auto port = parse_port(require_value(argc, argv, index, arg));
        if (!port)
            throw std::runtime_error("invalid port");

        out.port = port;
        return;
    }

    throw std::runtime_error("unknown option");
}

} // namespace

command_line_parser::command_line_parser(int argc, char** argv)
    : _argc(argc),
      _argv(argv)
{
}

command_line_result command_line_parser::parse() const
{
    cli_options out;

    for (int i = 1; i < _argc; ++i) {
        const std::string_view arg(_argv[i]);

        if (arg == "--help")
            return command_line_result{.show_help = true};
        if (arg == "--version")
            return command_line_result{.show_version = true};

        if (!arg.empty() && arg.front() == '-') {
            parse_named_option(arg, out, _argc, _argv, i);
            continue;
        }

        const auto port = parse_port(arg);
        if (!port)
            throw std::runtime_error("invalid port");

        out.port = port;
    }

    return command_line_result{
        .config = finalize_options(out,
                                   (_argc > 0 && _argv[0]) ? fs::path(_argv[0])
                                                           : fs::path{}),
    };
}

std::string command_line_parser::usage() const
{
    const auto program_name = (_argc > 0 && _argv[0]) ? _argv[0] : "garcon";

    return "usage: " + std::string(program_name) +
           " [port] [--bind ADDRESS] [--port PORT]"
           " [--modules-dir DIR]"
           " [--workers N] [--queue-capacity N]"
           " [--tls-cert PATH --tls-key PATH]"
           " [--help] [--version]\n";
}

} // namespace app

// Server configuration model.
//
// This file defines the app::server_config structure used to configure the
// listener address, port, document root, concurrency settings, and optional
// TLS credentials for a Garçon server instance.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include <filesystem>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace app {

struct tls_config
{
    std::filesystem::path certificate_file;
    std::filesystem::path private_key_file;
};

struct server_config
{
    // IPv4 address to bind the listening socket to.
    std::string bind_address = "127.0.0.1";
    // TCP port exposed by the listener.
    int port = 8080;
    // Default document root used by the static-files module.
    std::filesystem::path www_root = "www";
    // Directory containing the ordered module configuration files.
    std::filesystem::path module_config_dir = "modules.d";
    // Fixed worker-pool size used for concurrent connection handling.
    std::size_t worker_threads = 4;
    // Maximum number of accepted sockets buffered ahead of workers.
    std::size_t connection_queue_capacity = 256;
    // Optional TLS material; when present Garcon serves HTTPS.
    std::optional<tls_config> tls;

    [[nodiscard]] bool https_enabled() const noexcept
    {
        return tls.has_value();
    }

    [[nodiscard]] std::string_view scheme() const noexcept
    {
        return https_enabled() ? "https" : "http";
    }
};

} // namespace app

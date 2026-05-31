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
    std::string bind_address = "127.0.0.1";
    int port = 8080;
    std::filesystem::path www_root = "www";
    std::filesystem::path module_config_dir = "modules.d";
    std::size_t worker_threads = 4;
    std::size_t connection_queue_capacity = 256;
    std::optional<tls_config> tls;

    bool https_enabled() const
    {
        return tls.has_value();
    }

    std::string_view scheme() const
    {
        return https_enabled() ? "https" : "http";
    }
};

} // namespace app

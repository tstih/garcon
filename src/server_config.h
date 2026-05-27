// Server configuration model.
//
// This file defines the app::server_config structure used to configure the
// listener address, port, document root, and optional TLS credentials for a
// Garçon server instance.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include <filesystem>
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

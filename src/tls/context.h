// OpenSSL server context wrapper.
//
// This file defines the tls::context class, which owns a configured OpenSSL
// SSL_CTX object for Garçon's server-side HTTPS mode.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include <filesystem>
#include <memory>

#include <openssl/ssl.h>

namespace tls {

class context
{
public:
    context(std::filesystem::path certificate_file,
            std::filesystem::path private_key_file);

    context(const context&) = delete;
    context& operator=(const context&) = delete;
    context(context&&) noexcept = default;
    context& operator=(context&&) noexcept = default;

    SSL_CTX* native_handle() const;

private:
    using ssl_ctx_ptr = std::unique_ptr<SSL_CTX, decltype(&::SSL_CTX_free)>;

    ssl_ctx_ptr _ctx;
};

} // namespace tls

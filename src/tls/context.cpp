// Implementation of the OpenSSL server context wrapper.
//
// This file configures the shared TLS server state used by all HTTPS
// connections, including certificate loading, private-key loading, and
// conservative protocol defaults.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "tls/context.h"
#include "tls/tls_error.h"

#include <stdexcept>

#include <openssl/err.h>

namespace tls {

namespace {

void configure_context(SSL_CTX* ctx)
{
    if (::SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION) != 1)
        throw std::runtime_error(ssl_error_message("failed to set TLS minimum version"));

    long options = SSL_OP_NO_COMPRESSION;
#ifdef SSL_OP_NO_RENEGOTIATION
    options |= SSL_OP_NO_RENEGOTIATION;
#endif
    ::SSL_CTX_set_options(ctx, options);
}

void load_certificate(SSL_CTX* ctx, const std::filesystem::path& certificate_file)
{
    if (::SSL_CTX_use_certificate_file(ctx,
                                       certificate_file.string().c_str(),
                                       SSL_FILETYPE_PEM) != 1) {
        throw std::runtime_error(ssl_error_message("failed to load TLS certificate"));
    }
}

void load_private_key(SSL_CTX* ctx, const std::filesystem::path& private_key_file)
{
    if (::SSL_CTX_use_PrivateKey_file(ctx,
                                      private_key_file.string().c_str(),
                                      SSL_FILETYPE_PEM) != 1) {
        throw std::runtime_error(ssl_error_message("failed to load TLS private key"));
    }
}

void verify_private_key(SSL_CTX* ctx)
{
    if (::SSL_CTX_check_private_key(ctx) != 1)
        throw std::runtime_error(ssl_error_message("TLS private key does not match certificate"));
}

} // namespace

context::context(std::filesystem::path certificate_file,
                 std::filesystem::path private_key_file)
    : _ctx(::SSL_CTX_new(::TLS_server_method()), &::SSL_CTX_free)
{
    if (!_ctx)
        throw std::runtime_error(ssl_error_message("failed to create TLS context"));

    configure_context(_ctx.get());
    load_certificate(_ctx.get(), certificate_file);
    load_private_key(_ctx.get(), private_key_file);
    verify_private_key(_ctx.get());
}

SSL_CTX* context::native_handle() const
{
    return _ctx.get();
}

} // namespace tls

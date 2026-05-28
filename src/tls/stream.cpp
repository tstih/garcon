// Implementation of the TLS stream adapter.
//
// This file implements transport operations on top of OpenSSL while exposing
// the same small stream interface used by the rest of the server pipeline.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "tls/stream.h"
#include "tls/tls_error.h"

#include <cerrno>
#include <memory>
#include <stdexcept>

#include <openssl/err.h>

namespace tls {

namespace {

net::io_status map_ssl_status(SSL* ssl, int rc)
{
    const auto error = ::SSL_get_error(ssl, rc);

    switch (error) {
    case SSL_ERROR_ZERO_RETURN:
        return net::io_status::closed;
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
        return net::io_status::timeout;
    case SSL_ERROR_SYSCALL:
        if (errno == 0)
            return net::io_status::closed;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return net::io_status::timeout;
        return net::io_status::error;
    default:
        return net::io_status::error;
    }
}

SSL* create_ssl(SSL_CTX* ctx, int fd)
{
    using ssl_ptr = std::unique_ptr<SSL, decltype(&::SSL_free)>;
    ssl_ptr ssl(::SSL_new(ctx), &::SSL_free);
    if (!ssl)
        throw std::runtime_error(ssl_error_message("failed to create TLS connection state"));

    if (::SSL_set_fd(ssl.get(), fd) != 1)
        throw std::runtime_error(ssl_error_message("failed to bind TLS state to socket"));

    return ssl.release();
}

} // namespace

stream::stream(const context& tls_context, net::socket socket)
    : _socket(std::move(socket)),
      _ssl(create_ssl(tls_context.native_handle(), _socket.fd()), &::SSL_free)
{
}

bool stream::is_valid() const
{
    return _socket.is_valid() && static_cast<bool>(_ssl);
}

void stream::close()
{
    _ssl.reset();
    _socket.close();
}

bool stream::set_receive_timeout(std::chrono::milliseconds timeout)
{
    return _socket.set_receive_timeout(timeout);
}

bool stream::set_send_timeout(std::chrono::milliseconds timeout)
{
    return _socket.set_send_timeout(timeout);
}

net::io_status stream::handshake()
{
    const int rc = ::SSL_accept(_ssl.get());
    if (rc == 1)
        return net::io_status::ok;

    return map_ssl_status(_ssl.get(), rc);
}

net::read_result stream::recv_some(std::span<std::byte> out)
{
    std::size_t bytes_read = 0;
    const int rc = ::SSL_read_ex(_ssl.get(), out.data(), out.size(), &bytes_read);
    if (rc == 1) {
        return net::read_result{
            .status = net::io_status::ok,
            .bytes_read = bytes_read,
        };
    }

    return net::read_result{
        .status = map_ssl_status(_ssl.get(), rc),
    };
}

net::io_status stream::send_all(std::span<const std::byte> data)
{
    std::size_t sent = 0;

    while (sent < data.size()) {
        std::size_t bytes_written = 0;
        const int rc = ::SSL_write_ex(_ssl.get(),
                                      data.data() + sent,
                                      data.size() - sent,
                                      &bytes_written);
        if (rc != 1)
            return map_ssl_status(_ssl.get(), rc);

        sent += bytes_written;
    }

    return net::io_status::ok;
}

} // namespace tls

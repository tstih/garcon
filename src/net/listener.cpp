// Implementation of the TCP listening socket abstraction.
//
// This file implements the net::listener class, which is responsible for
// creating a listening socket, binding it to a TCP port, and accepting
// incoming client connections.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "net/listener.h"

#include <cerrno>
#include <cstdint>
#include <stdexcept>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace net {

namespace {

bool is_fatal_accept_error(int error)
{
    switch (error) {
    case EBADF:
    case EFAULT:
    case EINVAL:
    case ENOTSOCK:
    case EOPNOTSUPP:
        return true;
    default:
        return false;
    }
}

accept_result make_accept_error(int error)
{
    return accept_result{
        .status = is_fatal_accept_error(error)
            ? accept_status::fatal_error
            : accept_status::retryable_error,
        .error = std::error_code(error, std::generic_category()),
    };
}

} // namespace

listener::listener(std::string bind_address, int port)
    : _bind_address(std::move(bind_address))
{
    _sock = socket(::socket(AF_INET, SOCK_STREAM, 0));
    if (!_sock.valid())
        throw std::runtime_error("socket() failed");

    int yes = 1;
    if (::setsockopt(_sock.fd(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0)
        throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));

    if (::inet_pton(AF_INET, _bind_address.c_str(), &addr.sin_addr) != 1)
        throw std::runtime_error("invalid IPv4 bind address");

    if (::bind(_sock.fd(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
        throw std::runtime_error("bind() failed");

    if (::listen(_sock.fd(), 16) != 0)
        throw std::runtime_error("listen() failed");
}

accept_result listener::accept()
{
    sockaddr_in client{};
    socklen_t len = sizeof(client);

    for (;;) {
        const int cfd = ::accept(_sock.fd(),
                                 reinterpret_cast<sockaddr*>(&client),
                                 &len);
        if (cfd < 0 && errno == EINTR)
            continue;
        if (cfd < 0)
            return make_accept_error(errno);

        return accept_result{
            .status = accept_status::accepted,
            .client = socket(cfd),
        };
    }
}

} // namespace net

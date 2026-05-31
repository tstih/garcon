// Implementation of the net::socket TCP socket wrapper.
//
// This file implements the low-level mechanics of socket ownership,
// send and receive operations, and safe transfer of ownership via
// move semantics.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "net/socket.h"

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <utility>

#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace net {

namespace {

timeval to_timeval(std::chrono::milliseconds timeout)
{
    if (timeout.count() < 0)
        timeout = std::chrono::milliseconds::zero();

    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(timeout);
    const auto usecs = std::chrono::duration_cast<std::chrono::microseconds>(timeout - secs);

    return timeval{
        .tv_sec = static_cast<decltype(timeval::tv_sec)>(secs.count()),
        .tv_usec = static_cast<decltype(timeval::tv_usec)>(usecs.count()),
    };
}

io_status status_from_errno(int error)
{
    if (error == EAGAIN || error == EWOULDBLOCK)
        return io_status::timeout;
    return io_status::error;
}

} // namespace

socket::socket() noexcept : _fd(-1) {}

socket::socket(int fd, std::string peer_address)
    : _fd(fd),
      _peer_address(std::move(peer_address))
{
}

socket::socket(socket&& o) noexcept
    : _fd(o._fd),
      _peer_address(std::move(o._peer_address))
{
    o._fd = -1;
}

socket& socket::operator=(socket&& o) noexcept
{
    if (this != &o) {
        close();
        _fd = o._fd;
        _peer_address = std::move(o._peer_address);
        o._fd = -1;
    }
    return *this;
}

socket::~socket()
{
    close();
}

bool socket::is_valid() const noexcept
{
    return _fd >= 0;
}

int socket::fd() const noexcept
{
    return _fd;
}

std::string_view socket::peer_address() const noexcept
{
    return _peer_address;
}

void socket::close()
{
    if (_fd >= 0) {
        ::shutdown(_fd, SHUT_WR);
        ::close(_fd);
        _fd = -1;
    }
}

bool socket::set_receive_timeout(std::chrono::milliseconds timeout)
{
    const auto tv = to_timeval(timeout);
    return ::setsockopt(_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
}

bool socket::set_send_timeout(std::chrono::milliseconds timeout)
{
    const auto tv = to_timeval(timeout);
    return ::setsockopt(_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == 0;
}

read_result socket::recv_some(std::span<std::byte> out)
{
    for (;;) {
        const auto rc = ::recv(_fd, out.data(), out.size(), 0);
        if (rc < 0 && errno == EINTR)
            continue;

        if (rc > 0) {
            return read_result{
                .status = io_status::ok,
                .bytes_read = static_cast<std::size_t>(rc),
            };
        }

        if (rc == 0)
            return read_result{.status = io_status::closed};

        return read_result{.status = status_from_errno(errno)};
    }
}

io_status socket::send_all(std::span<const std::byte> data)
{
    std::size_t sent = 0;

    // Loop until the entire buffer has been written to the socket.
    while (sent < data.size()) {
#ifdef MSG_NOSIGNAL
        constexpr int flags = MSG_NOSIGNAL;
#else
        constexpr int flags = 0;
#endif
        const auto rc = ::send(_fd,
                               data.data() + sent,
                               data.size() - sent,
                               flags);
        if (rc < 0 && errno == EINTR)
            continue;
        if (rc == 0)
            return io_status::closed;
        if (rc < 0)
            return status_from_errno(errno);

        sent += static_cast<std::size_t>(rc);
    }

    return io_status::ok;
}

io_status socket::send_all(std::string_view s)
{
    const auto* p = reinterpret_cast<const std::byte*>(s.data());
    return send_all(std::span<const std::byte>(p, s.size()));
}

} // namespace net

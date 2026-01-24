#include "net_socket.h"

#include <sys/socket.h>
#include <unistd.h>

namespace net {

socket::socket() : _fd(-1) {}

socket::socket(int fd) : _fd(fd) {}

socket::socket(socket&& o) noexcept : _fd(o._fd)
{
    o._fd = -1;
}

socket& socket::operator=(socket&& o) noexcept
{
    if (this != &o) {
        close();
        _fd = o._fd;
        o._fd = -1;
    }
    return *this;
}

socket::~socket()
{
    close();
}

bool socket::valid() const
{
    return _fd >= 0;
}

int socket::fd() const
{
    return _fd;
}

void socket::close()
{
    if (_fd >= 0) {
        ::close(_fd);
        _fd = -1;
    }
}

std::ptrdiff_t socket::recv_some(std::span<std::byte> out)
{
    const auto rc = ::recv(_fd, out.data(), out.size(), 0);
    return static_cast<std::ptrdiff_t>(rc);
}

bool socket::send_all(std::span<const std::byte> data)
{
    std::size_t sent = 0;

    while (sent < data.size()) {
        const auto rc = ::send(_fd,
                               data.data() + sent,
                               data.size() - sent,
                               0);
        if (rc <= 0) return false;
        sent += static_cast<std::size_t>(rc);
    }

    return true;
}

bool socket::send_all(std::string_view s)
{
    const auto* p = reinterpret_cast<const std::byte*>(s.data());
    return send_all(std::span<const std::byte>(p, s.size()));
}

} /* namespace net */

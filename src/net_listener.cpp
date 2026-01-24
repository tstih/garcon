#include "net_listener.h"

#include <cstdint>
#include <stdexcept>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace net {

listener::listener(int port)
{
    _sock = socket(::socket(AF_INET, SOCK_STREAM, 0));
    if (!_sock.valid())
        throw std::runtime_error("socket() failed");

    int yes = 1;
    ::setsockopt(_sock.fd(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<std::uint16_t>(port));

    if (::bind(_sock.fd(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
        throw std::runtime_error("bind() failed");

    if (::listen(_sock.fd(), 16) != 0)
        throw std::runtime_error("listen() failed");
}

socket listener::accept()
{
    sockaddr_in client{};
    socklen_t len = sizeof(client);

    const int cfd = ::accept(_sock.fd(), reinterpret_cast<sockaddr*>(&client), &len);
    return socket(cfd);
}

} /* namespace net */

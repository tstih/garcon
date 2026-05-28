// Implementation of the plain TCP stream adapter.
//
// This file implements net::plain_stream by forwarding transport operations to
// the owned net::socket. Plain TCP requires no handshake, so it satisfies the
// stream contract with a simple pass-through implementation.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "net/plain_stream.h"

namespace net {

plain_stream::plain_stream(socket socket)
    : _socket(std::move(socket))
{
}

bool plain_stream::is_valid() const
{
    return _socket.is_valid();
}

void plain_stream::close()
{
    _socket.close();
}

bool plain_stream::set_receive_timeout(std::chrono::milliseconds timeout)
{
    return _socket.set_receive_timeout(timeout);
}

bool plain_stream::set_send_timeout(std::chrono::milliseconds timeout)
{
    return _socket.set_send_timeout(timeout);
}

io_status plain_stream::handshake()
{
    return io_status::ok;
}

read_result plain_stream::recv_some(std::span<std::byte> out)
{
    return _socket.recv_some(out);
}

io_status plain_stream::send_all(std::span<const std::byte> data)
{
    return _socket.send_all(data);
}

} // namespace net

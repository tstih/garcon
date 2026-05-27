// Minimal RAII wrapper for a TCP socket file descriptor.
//
// This file defines the net::socket class, which owns a single socket file
// descriptor and is responsible for closing it exactly once. Copying is
// disabled to prevent shared ownership, while move semantics allow explicit
// transfer of ownership between objects.
//
// The interface provides raw byte send and receive operations only and
// deliberately contains no protocol-level logic.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "net/io.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace net {

class socket
{
public:
    // Constructs an empty socket object that does not own a file descriptor.
    // The socket is initially invalid and must later adopt a descriptor.
    socket();

    // Constructs a socket object by explicitly adopting ownership of an
    // already-open socket file descriptor.
    explicit socket(int fd);

    // Prevent accidental assignments.
    socket(const socket&) = delete;
    socket& operator=(const socket&) = delete;

    // Transfers ownership of the socket file descriptor from another socket.
    // The moved-from socket becomes invalid.
    socket(socket&& o) noexcept;

    // Releases any currently owned descriptor and transfers ownership from
    // another socket. The moved-from socket becomes invalid.
    socket& operator=(socket&& o) noexcept;

    // Destroys the socket object and closes the owned file descriptor,
    // if any.
    ~socket();

    // Returns true if this object currently owns a valid socket descriptor.
    bool valid() const;

    // Returns the underlying socket file descriptor.
    // Intended for low-level operations only.
    int  fd() const;

    // Closes the owned socket descriptor, if any, and marks the socket invalid.
    // Safe to call multiple times.
    void close();

    // Sets a receive timeout for blocking reads on this socket.
    bool set_receive_timeout(std::chrono::milliseconds timeout);

    // Sets a send timeout for blocking writes on this socket.
    bool set_send_timeout(std::chrono::milliseconds timeout);

    // Receives up to out.size() bytes from the socket.
    // Returns a status plus the number of bytes received on success.
    read_result recv_some(std::span<std::byte> out);

    // Sends the entire buffer to the socket.
    io_status send_all(std::span<const std::byte> data);
    io_status send_all(std::string_view s);

private:
    int _fd;
};

} // namespace net

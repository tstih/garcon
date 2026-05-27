// TCP listening socket abstraction.
//
// This file defines the net::listener class, which owns a listening TCP socket
// bound to a specific port. The listener is responsible for accepting incoming
// connections and reporting whether each accept attempt succeeded, failed
// transiently, or failed fatally.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "net/socket.h"

#include <string>
#include <system_error>

namespace net {

enum class accept_status
{
    accepted,
    retryable_error,
    fatal_error,
};

struct accept_result
{
    accept_status status = accept_status::retryable_error;
    socket client;
    std::error_code error;

    explicit operator bool() const
    {
        return status == accept_status::accepted;
    }
};

class listener
{
public:
    // Creates a listening socket bound to the given TCP port and starts
    // listening for incoming connections.
    // Throws an exception if socket creation, binding, or listening fails.
    listener(std::string bind_address, int port);

    // Accepts an incoming connection.
    // On success, returns a connected client socket. On failure, returns an
    // explicit status plus the corresponding system error code.
    accept_result accept();

private:
    std::string _bind_address;

    // Listening socket that owns the bound socket descriptor.
    socket _sock;
};

} // namespace net

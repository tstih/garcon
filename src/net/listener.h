// TCP listening socket abstraction.
//
// This file defines the net::listener class, which owns a listening TCP socket
// bound to a specific port. The listener is responsible for accepting incoming
// connections and transferring ownership of each accepted connection to a
// net::socket instance.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "net/socket.h"

namespace net {

class listener
{
public:
    // Creates a listening socket bound to the given TCP port and starts
    // listening for incoming connections.
    // Throws an exception if socket creation, binding, or listening fails.
    explicit listener(int port);

    // Accepts an incoming connection.
    // On success, returns a socket object that owns the connected socket
    // descriptor. The returned socket represents a single client connection.
    socket accept();

private:
    // Listening socket that owns the bound socket descriptor.
    socket _sock;
};

} // namespace net
// Abstract stream factory for accepted connections.
//
// This interface hides the choice between plain TCP and TLS stream types from
// the server runtime. The server only depends on transport-neutral
// net::stream instances produced through this factory.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "net/socket.h"

#include <memory>

namespace net {
class stream;
}

namespace app {

class stream_factory
{
public:
    virtual ~stream_factory() = default;

    virtual std::unique_ptr<net::stream> create(net::socket client) const = 0;
};

} // namespace app

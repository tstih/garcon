// Abstract request handler contract.
//
// This interface keeps the server runtime independent from any concrete
// application behavior. The current root implementation is an ordered request
// pipeline loaded from shared modules, but future versions can still introduce
// routers or API handlers behind the same seam.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "http/request.h"
#include "http/response.h"

namespace app {

class request_handler
{
public:
    virtual ~request_handler() = default;

    [[nodiscard]] virtual http::response handle(const http::request& request) const = 0;
};

} // namespace app

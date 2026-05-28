// Ordered request pipeline for HTTP modules.
//
// The pipeline is the single root request handler used by the server. It
// calls modules in order until one responds, requests a future upgrade, or
// signals an error.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "app/request_handler.h"
#include "app/request_module.h"

#include <memory>
#include <vector>

namespace app {

class request_pipeline final : public request_handler
{
public:
    void add_module(std::unique_ptr<request_module> module);

    http::response handle(const http::request& request) const override;

private:
    std::vector<std::unique_ptr<request_module>> _modules;
};

} // namespace app

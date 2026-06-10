// Router shared module implementation.
//
// This file defines the C++ module implementation for a richer gateway-style
// router supporting method + target patterns with path parameters (e.g.
// /users/{id}, /api/{version}/items/{item_id}). Captured values are injected
// as X-Garcon-Route-Param-* headers on `pass` results so downstream modules
// can consume them via the existing header accumulation mechanism.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "garcon/module_cpp.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace garcon::modules {

class router_module final : public garcon::module::module
{
public:
    router_module(const garcon::module::host_context& host,
                  std::string_view config_text);

    garcon::module::result handle(const http::request& request) const override;

    enum class pattern_kind
    {
        exact,
        prefix,
    };

    struct route_entry
    {
        std::string method_pattern;
        std::string target_pattern;
        pattern_kind target_kind = pattern_kind::exact;
        garcon::module::outcome outcome = garcon::module::outcome::pass;
        std::optional<http::response> response;
    };

private:
    std::vector<route_entry> _routes;
};

} // namespace garcon::modules

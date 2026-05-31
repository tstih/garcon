// Route-table shared module implementation.
//
// This file defines the C++ module implementation for a small gateway-style
// route table that can respond directly or pass requests onward.
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

class route_table_module final : public garcon::module::module
{
public:
    route_table_module(const garcon::module::host_context& host,
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

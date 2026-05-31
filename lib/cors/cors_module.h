// CORS shared module implementation.
//
// This file defines the C++ module implementation for a small gateway-style
// CORS policy module.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "garcon/module_cpp.h"

#include <string>
#include <string_view>
#include <vector>

namespace garcon::modules {

class cors_module final : public garcon::module::module
{
public:
    cors_module(const garcon::module::host_context& host,
                std::string_view config_text);

    garcon::module::result handle(const http::request& request) const override;

    enum class pattern_kind
    {
        exact,
        prefix,
    };

    struct origin_pattern
    {
        std::string value;
        bool allow_any = false;
    };

    struct rule
    {
        std::string target_pattern;
        pattern_kind target_kind = pattern_kind::exact;
        std::vector<origin_pattern> allowed_origins;
        std::vector<std::string> allowed_methods;
        std::string allow_methods_header;
        std::string allow_headers_header;
    };

private:
    std::vector<rule> _rules;
};

} // namespace garcon::modules

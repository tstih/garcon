// Header-guard shared module implementation.
//
// This file defines the C++ module implementation for a small gateway-style
// header guard that protects configured request paths.
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

class header_guard_module final : public garcon::module::module
{
public:
    header_guard_module(const garcon::module::host_context& host,
                        std::string_view config_text);

    garcon::module::result handle(const http::request& request) const override;

    enum class pattern_kind
    {
        exact,
        prefix,
    };

    struct guard_rule
    {
        std::string method_pattern;
        std::string target_pattern;
        pattern_kind target_kind = pattern_kind::exact;
        std::string header_name;
        std::optional<std::string> expected_value;
        int failure_status = 401;
        std::string failure_body;
    };

private:
    std::vector<guard_rule> _rules;
};

} // namespace garcon::modules

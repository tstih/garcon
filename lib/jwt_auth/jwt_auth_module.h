// JWT-auth shared module implementation.
//
// This file defines the C++ module implementation for a small gateway-style
// JWT / Bearer token authentication module.
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

class jwt_auth_module final : public garcon::module::module
{
public:
    jwt_auth_module(const garcon::module::host_context& host,
                    std::string_view config_text);

    garcon::module::result handle(const http::request& request) const override;

    enum class pattern_kind
    {
        exact,
        prefix,
    };

    struct auth_rule
    {
        std::string method_pattern;
        std::string target_pattern;
        pattern_kind target_kind = pattern_kind::exact;
    };

private:
    std::string _secret;
    std::string _alg;
    std::optional<std::string> _issuer;
    std::optional<std::string> _audience;
    std::vector<auth_rule> _rules;
};

} // namespace garcon::modules

// Rate limit shared module implementation.
//
// This file defines the C++ module for a simple token-bucket rate limiter
// that can be placed in the request pipeline to protect routes.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "garcon/module_cpp.h"

#include <chrono>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace garcon::modules {

class rate_limit_module final : public garcon::module::module
{
public:
    rate_limit_module(const garcon::module::host_context& host,
                      std::string_view config_text);

    garcon::module::result handle(const http::request& request) const override;

    enum class pattern_kind
    {
        exact,
        prefix,
    };

    enum class key_kind
    {
        global,
        header,
    };

    struct rule
    {
        std::string method_pattern;
        std::string target_pattern;
        pattern_kind target_kind = pattern_kind::exact;
        key_kind key_type = key_kind::header;
        std::string key_name; // header name when key_type == header
        int limit = 0;
        int window_seconds = 0;
        int burst = 0;
    };

private:
    struct token_bucket
    {
        double tokens = 0.0;
        std::chrono::steady_clock::time_point last_refill{};
    };

    std::string make_state_key(std::size_t rule_index, std::string_view subject) const;

    std::optional<http::response> check_and_consume(const rule& r,
                                                    std::size_t rule_index,
                                                    std::string_view subject) const;

    std::vector<rule> _rules;

    mutable std::mutex _mutex;
    mutable std::unordered_map<std::string, token_bucket> _buckets;
};

} // namespace garcon::modules

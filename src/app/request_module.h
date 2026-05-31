// Ordered request-module contract for the Garcon pipeline.
//
// Each module receives a parsed HTTP request and decides whether to pass the
// request onward, respond immediately, request a future connection upgrade, or
// signal an internal error. The server core stays simple by evaluating these
// outcomes in order.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "http/request.h"
#include "http/response.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace app {

enum class module_outcome
{
    pass,
    respond,
    upgrade,
    error,
};

struct module_result
{
    module_outcome outcome = module_outcome::pass;
    std::optional<http::response> response;
    std::vector<http::header_field> response_headers;

    static module_result pass()
    {
        return module_result{.outcome = module_outcome::pass};
    }

    static module_result respond(http::response r)
    {
        return module_result{
            .outcome = module_outcome::respond,
            .response = std::move(r),
        };
    }

    static module_result upgrade()
    {
        return module_result{.outcome = module_outcome::upgrade};
    }

    static module_result error()
    {
        return module_result{.outcome = module_outcome::error};
    }

    module_result& add_response_header(std::string_view name, std::string_view value)
    {
        response_headers.push_back(http::header_field{
            .name = std::string(name),
            .value = std::string(value),
        });
        return *this;
    }
};

class request_module
{
public:
    virtual ~request_module() = default;

    virtual std::string_view name() const = 0;
    virtual module_result handle(const http::request& request) const = 0;
};

} // namespace app

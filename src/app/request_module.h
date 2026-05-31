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

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace app {

struct pass_result
{
    std::vector<http::header_field> response_headers;
};

struct respond_result
{
    http::response response;
};

struct upgrade_result
{
    std::vector<http::header_field> response_headers;
};

struct error_result
{
    std::vector<http::header_field> response_headers;
};

class module_result
{
public:
    using variant_type =
        std::variant<pass_result, respond_result, upgrade_result, error_result>;

    static module_result pass()
    {
        return module_result(pass_result{});
    }

    static module_result respond(http::response r)
    {
        return module_result(respond_result{.response = std::move(r)});
    }

    static module_result upgrade()
    {
        return module_result(upgrade_result{});
    }

    static module_result error()
    {
        return module_result(error_result{});
    }

    module_result& add_response_header(std::string_view name, std::string_view value)
    {
        auto header = http::header_field{
            .name = std::string(name),
            .value = std::string(value),
        };

        std::visit(
            [&header](auto& result) {
                using result_type = std::remove_cvref_t<decltype(result)>;
                if constexpr (std::is_same_v<result_type, respond_result>) {
                    result.response.headers.push_back(std::move(header));
                } else {
                    result.response_headers.push_back(std::move(header));
                }
            },
            _value);
        return *this;
    }

    [[nodiscard]] const variant_type& as_variant() const noexcept
    {
        return _value;
    }

private:
    explicit module_result(variant_type value) : _value(std::move(value)) {}

    variant_type _value;
};

class request_module
{
public:
    virtual ~request_module() = default;

    [[nodiscard]] virtual std::string_view name() const = 0;
    [[nodiscard]] virtual module_result handle(const http::request& request) const = 0;
};

} // namespace app

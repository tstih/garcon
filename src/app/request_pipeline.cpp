// Implementation of the ordered request pipeline.
//
// This file contains the small pipeline executor that applies modules in the
// configured order and turns their outcomes into the final HTTP response.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "app/request_pipeline.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace app {

namespace {

void append_response_headers(http::response& response,
                             const std::vector<http::header_field>& headers)
{
    response.headers.insert(response.headers.end(), headers.begin(), headers.end());
}

http::response finalize_response(http::response response,
                                 const std::vector<http::header_field>& pending_headers)
{
    append_response_headers(response, pending_headers);
    return response;
}

} // namespace

void request_pipeline::add_module(std::unique_ptr<request_module> module)
{
    if (!module)
        throw std::invalid_argument("request pipeline module must not be null");

    _modules.push_back(std::move(module));
}

http::response request_pipeline::handle(const http::request& request) const
{
    std::vector<http::header_field> pending_headers;

    for (const auto& module : _modules) {
        const auto result = module->handle(request);
        const auto response = std::visit(
            [&](const auto& result_value) -> std::optional<http::response> {
                using result_type = std::remove_cvref_t<decltype(result_value)>;

                if constexpr (std::is_same_v<result_type, pass_result>) {
                    pending_headers.insert(pending_headers.end(),
                                           result_value.response_headers.begin(),
                                           result_value.response_headers.end());
                    return std::nullopt;
                } else if constexpr (std::is_same_v<result_type, respond_result>) {
                    return finalize_response(result_value.response, pending_headers);
                } else if constexpr (std::is_same_v<result_type, upgrade_result>) {
                    pending_headers.insert(pending_headers.end(),
                                           result_value.response_headers.begin(),
                                           result_value.response_headers.end());
                    return finalize_response(http::response::text(
                                                 501,
                                                 "Not Implemented",
                                                 "connection upgrade not implemented\n"),
                                             pending_headers);
                } else {
                    pending_headers.insert(pending_headers.end(),
                                           result_value.response_headers.begin(),
                                           result_value.response_headers.end());
                    return finalize_response(http::response::text(
                                                 500,
                                                 "Internal Server Error",
                                                 "module error\n"),
                                             pending_headers);
                }
            },
            result.as_variant());

        if (response)
            return *response;
    }

    return finalize_response(http::response::text(404, "Not Found", "not found\n"),
                             pending_headers);
}

} // namespace app

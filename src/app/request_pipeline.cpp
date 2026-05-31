// Implementation of the ordered request pipeline.
//
// This file contains the small pipeline executor that applies modules in the
// configured order and turns their outcomes into the final HTTP response.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "app/request_pipeline.h"

#include <stdexcept>
#include <string>
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

        pending_headers.insert(pending_headers.end(),
                               result.response_headers.begin(),
                               result.response_headers.end());

        switch (result.outcome) {
        case module_outcome::pass:
            continue;
        case module_outcome::respond:
            if (!result.response)
                throw std::logic_error(
                    std::string("module '") +
                    std::string(module->name()) +
                    "' returned outcome::respond without a response");
            return finalize_response(*result.response, pending_headers);
        case module_outcome::upgrade:
            return finalize_response(http::response::text(501,
                                                          "Not Implemented",
                                                          "connection upgrade not implemented\n"),
                                     pending_headers);
        case module_outcome::error:
            return finalize_response(http::response::text(500,
                                                          "Internal Server Error",
                                                          "module error\n"),
                                     pending_headers);
        }
    }

    return finalize_response(http::response::text(404, "Not Found", "not found\n"),
                             pending_headers);
}

} // namespace app

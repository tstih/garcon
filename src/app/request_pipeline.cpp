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

namespace app {

void request_pipeline::add_module(std::unique_ptr<request_module> module)
{
    if (!module)
        throw std::invalid_argument("request pipeline module must not be null");

    _modules.push_back(std::move(module));
}

http::response request_pipeline::handle(const http::request& request) const
{
    for (const auto& module : _modules) {
        const auto result = module->handle(request);

        switch (result.outcome) {
        case module_outcome::pass:
            continue;
        case module_outcome::respond:
            if (!result.response)
                throw std::logic_error(
                    std::string("module '") +
                    std::string(module->name()) +
                    "' returned outcome::respond without a response");
            return *result.response;
        case module_outcome::upgrade:
            return http::response::text(501,
                                        "Not Implemented",
                                        "connection upgrade not implemented\n");
        case module_outcome::error:
            return http::response::text(500,
                                        "Internal Server Error",
                                        "module error\n");
        }
    }

    return http::response::text(404, "Not Found", "not found\n");
}

} // namespace app

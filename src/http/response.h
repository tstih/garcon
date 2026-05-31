// HTTP response representation.
//
// This file defines the http::response structure, which represents an HTTP
// response message produced by the server. It contains the status code,
// reason phrase, optional content type, and response body, and provides
// serialization into a wire-format HTTP/1.1 response.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "http/header.h"

#include <optional>
#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace http {

struct response
{
    int status = 200;
    std::string reason = "OK";
    std::string content_type;
    std::vector<header_field> headers;
    std::string body;
    std::optional<std::size_t> content_length;

    const header_field* find_header(std::string_view name) const
    {
        const auto it = std::find_if(headers.begin(),
                                     headers.end(),
                                     [name](const header_field& header) {
                                         return header_name_equals(header.name, name);
                                     });

        return it == headers.end() ? nullptr : &*it;
    }

    void add_header(std::string_view name, std::string_view value)
    {
        headers.push_back(header_field{
            .name = std::string(name),
            .value = std::string(value),
        });
    }

    void set_header(std::string_view name, std::string_view value)
    {
        auto it = std::find_if(headers.begin(),
                               headers.end(),
                               [name](const header_field& header) {
                                   return header_name_equals(header.name, name);
                               });

        if (it != headers.end()) {
            it->value = std::string(value);
            return;
        }

        add_header(name, value);
    }

    // Serializes the response into an HTTP/1.1 response message.
    std::string serialize() const;

    // Constructs a plain-text response with the given status and message.
    static response text(int status,
                         std::string_view reason,
                         std::string_view msg);
};

} // namespace http

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
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace http {

enum class connection_policy
{
    close,
    keep_alive,
};

struct response
{
    int status = 200;
    std::string reason = "OK";
    std::string content_type;
    std::vector<header_field> headers;
    std::string body;
    std::optional<std::size_t> content_length;

    [[nodiscard]] const header_field* find_header(std::string_view name) const noexcept
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
        validate_header(name, value);
        headers.push_back(header_field{
            .name = std::string(name),
            .value = std::string(value),
        });
    }

    void set_header(std::string_view name, std::string_view value)
    {
        validate_header(name, value);
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
    [[nodiscard]] std::string serialize(
        connection_policy policy = connection_policy::close) const;

    // Constructs a plain-text response with the given status and message.
    [[nodiscard]] static response text(int status,
                                       std::string_view reason,
                                       std::string_view msg);

    [[nodiscard]] static bool valid_header_name(std::string_view name) noexcept;
    [[nodiscard]] static bool valid_header_value(std::string_view value) noexcept;

private:
    static void validate_header(std::string_view name, std::string_view value)
    {
        if (!valid_header_name(name))
            throw std::invalid_argument("invalid response header name");
        if (!valid_header_value(value))
            throw std::invalid_argument("invalid response header value");
    }
};

} // namespace http

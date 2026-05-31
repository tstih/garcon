// HTTP request representation.
//
// This file defines the http::request structure, which represents the
// minimal parsed form of an HTTP request needed by the server. Currently,
// it stores the request method and target after strictly validating the
// request line, leaving header fields and message bodies for future
// extension.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "http/header.h"

#include <algorithm>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace http {

enum class request_parse_error
{
    malformed_request_line,
    invalid_method,
    invalid_target,
    invalid_version,
    invalid_header,
};

struct request
{
    int version_major = 1;
    int version_minor = 1;
    std::string method;
    std::string target;
    std::vector<header_field> headers;

    [[nodiscard]] const header_field* find_header(std::string_view name) const noexcept
    {
        const auto it = std::find_if(headers.begin(),
                                     headers.end(),
                                     [name](const header_field& header) {
                                         return header_name_equals(header.name, name);
                                     });

        return it == headers.end() ? nullptr : &*it;
    }

    [[nodiscard]] std::optional<std::string_view> header_value(std::string_view name) const noexcept
    {
        const auto* header = find_header(name);
        if (!header)
            return std::nullopt;

        return header->value;
    }

    [[nodiscard]] bool header_equals(std::string_view name,
                                     std::string_view value) const noexcept
    {
        const auto* header = find_header(name);
        return header && header->value == value;
    }

    [[nodiscard]] bool connection_token_present(std::string_view token) const noexcept;
    [[nodiscard]] bool keep_alive_requested() const noexcept;
    [[nodiscard]] std::string version_string() const;

    // Parses and validates the HTTP request line and header fields from a
    // header block. Only origin-form targets with HTTP/1.0 or HTTP/1.1 are
    // accepted.
    [[nodiscard]] static std::expected<request, request_parse_error> parse(
        std::string_view header_block);
};

} // namespace http

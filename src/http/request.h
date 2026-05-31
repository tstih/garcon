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

#include <optional>
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace http {

struct request
{
    std::string method;
    std::string target;
    std::vector<header_field> headers;

    const header_field* find_header(std::string_view name) const
    {
        const auto it = std::find_if(headers.begin(),
                                     headers.end(),
                                     [name](const header_field& header) {
                                         return header_name_equals(header.name, name);
                                     });

        return it == headers.end() ? nullptr : &*it;
    }

    std::optional<std::string_view> header_value(std::string_view name) const
    {
        const auto* header = find_header(name);
        if (!header)
            return std::nullopt;

        return header->value;
    }

    bool header_equals(std::string_view name, std::string_view value) const
    {
        const auto* header = find_header(name);
        return header && header->value == value;
    }

    // Parses and validates the HTTP request line and header fields from a
    // header block. Only origin-form targets with HTTP/1.0 or HTTP/1.1 are
    // accepted.
    static std::optional<request> parse(std::string_view header_block);
};

} // namespace http

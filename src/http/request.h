// HTTP request representation.
//
// This file defines the http::request structure, which represents the
// minimal parsed form of an HTTP request needed by the server. Currently,
// it extracts only the request method and target from the request line,
// leaving header fields and message bodies for future extension.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace http {

struct request
{
    std::string method;
    std::string target;

    // Parses an HTTP request header block and extracts the request line.
    // Returns an empty optional if parsing fails.
    static std::optional<request> parse(std::string_view header_block);
};

} // namespace http

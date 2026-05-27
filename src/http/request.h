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

#include <optional>
#include <string>
#include <string_view>

namespace http {

struct request
{
    std::string method;
    std::string target;

    // Parses and validates the HTTP request line from a header block.
    // Only origin-form targets with HTTP/1.0 or HTTP/1.1 are accepted.
    static std::optional<request> parse(std::string_view header_block);
};

} // namespace http

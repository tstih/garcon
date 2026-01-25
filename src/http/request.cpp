// Implementation of HTTP request parsing.
//
// This file implements parsing logic for the http::request structure,
// extracting the request method and target from the HTTP request line.
// The implementation is intentionally minimal and ignores headers and
// message bodies.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "http/request.h"

namespace http {

std::optional<request> request::parse(std::string_view header_block)
{
    const auto eol = header_block.find("\r\n");
    const auto line = (eol == std::string_view::npos)
        ? header_block
        : header_block.substr(0, eol);

    const auto sp1 = line.find(' ');
    if (sp1 == std::string_view::npos)
        return std::nullopt;

    const auto sp2 = line.find(' ', sp1 + 1);
    if (sp2 == std::string_view::npos)
        return std::nullopt;

    request r;
    r.method.assign(line.substr(0, sp1));
    r.target.assign(line.substr(sp1 + 1, sp2 - (sp1 + 1)));
    return r;
}

} // namespace http

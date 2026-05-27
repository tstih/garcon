// Implementation of HTTP request parsing.
//
// This file implements parsing logic for the http::request structure,
// extracting the request method and target from a strictly validated HTTP
// request line. The implementation remains intentionally small and ignores
// headers and message bodies.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "http/request.h"

namespace http {

namespace {

constexpr bool is_alpha(unsigned char ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

constexpr bool is_digit(unsigned char ch)
{
    return ch >= '0' && ch <= '9';
}

constexpr bool is_token_char(unsigned char ch)
{
    switch (ch) {
    case '!':
    case '#':
    case '$':
    case '%':
    case '&':
    case '\'':
    case '*':
    case '+':
    case '-':
    case '.':
    case '^':
    case '_':
    case '`':
    case '|':
    case '~':
        return true;
    default:
        return is_alpha(ch) || is_digit(ch);
    }
}

bool is_valid_method(std::string_view method)
{
    if (method.empty())
        return false;

    for (const char ch : method) {
        if (!is_token_char(static_cast<unsigned char>(ch)))
            return false;
    }

    return true;
}

bool is_valid_target(std::string_view target)
{
    // Garcon only serves origin-form targets such as "/index.html".
    if (target.empty() || target[0] != '/')
        return false;

    for (const char ch : target) {
        const auto byte = static_cast<unsigned char>(ch);
        if (byte <= 0x20 || byte >= 0x7f)
            return false;
        if (ch == '#' || ch == '\\')
            return false;
    }

    return true;
}

bool is_valid_version(std::string_view version)
{
    return version == "HTTP/1.0" || version == "HTTP/1.1";
}

} // namespace

std::optional<request> request::parse(std::string_view header_block)
{
    const auto eol = header_block.find("\r\n");
    if (eol == std::string_view::npos)
        return std::nullopt;

    const auto line = header_block.substr(0, eol);

    const auto sp1 = line.find(' ');
    if (sp1 == std::string_view::npos)
        return std::nullopt;

    const auto sp2 = line.find(' ', sp1 + 1);
    if (sp2 == std::string_view::npos)
        return std::nullopt;
    if (line.find(' ', sp2 + 1) != std::string_view::npos)
        return std::nullopt;

    const auto method = line.substr(0, sp1);
    const auto target = line.substr(sp1 + 1, sp2 - (sp1 + 1));
    const auto version = line.substr(sp2 + 1);

    if (!is_valid_method(method) ||
        !is_valid_target(target) ||
        !is_valid_version(version)) {
        return std::nullopt;
    }

    request r;
    r.method.assign(method);
    r.target.assign(target);
    return r;
}

} // namespace http

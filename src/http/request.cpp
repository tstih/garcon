// Implementation of HTTP request parsing.
//
// This file implements parsing logic for the http::request structure,
// extracting the request method and target from a strictly validated HTTP
// request line and header fields. The implementation remains intentionally
// small and ignores request bodies.
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

bool is_optional_whitespace(char ch)
{
    return ch == ' ' || ch == '\t';
}

std::string_view trim_optional_whitespace(std::string_view value)
{
    std::size_t begin = 0;
    while (begin < value.size() && is_optional_whitespace(value[begin]))
        ++begin;

    std::size_t end = value.size();
    while (end > begin && is_optional_whitespace(value[end - 1]))
        --end;

    return value.substr(begin, end - begin);
}

bool is_valid_header_name(std::string_view name)
{
    if (name.empty())
        return false;

    for (const char ch : name) {
        if (!is_token_char(static_cast<unsigned char>(ch)))
            return false;
    }

    return true;
}

bool is_valid_header_value(std::string_view value)
{
    for (const char ch : value) {
        const auto byte = static_cast<unsigned char>(ch);
        if (ch == '\t')
            continue;
        if (byte < 0x20 || byte == 0x7f)
            return false;
    }

    return true;
}

bool parse_header_line(std::string_view line, header_field& header)
{
    if (line.empty() || is_optional_whitespace(line.front()))
        return false;

    const auto colon = line.find(':');
    if (colon == std::string_view::npos || colon == 0)
        return false;

    const auto name = line.substr(0, colon);
    const auto value = trim_optional_whitespace(line.substr(colon + 1));

    if (!is_valid_header_name(name) || !is_valid_header_value(value))
        return false;

    header.name.assign(name);
    header.value.assign(value);
    return true;
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

    std::size_t line_begin = eol + 2;
    while (line_begin <= header_block.size()) {
        const auto line_end = header_block.find("\r\n", line_begin);
        if (line_end == std::string_view::npos)
            return std::nullopt;

        if (line_end == line_begin)
            break;

        header_field header;
        if (!parse_header_line(header_block.substr(line_begin, line_end - line_begin),
                               header)) {
            return std::nullopt;
        }

        r.headers.push_back(std::move(header));
        line_begin = line_end + 2;
    }

    return r;
}

} // namespace http

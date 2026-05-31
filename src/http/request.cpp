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

#include <charconv>

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

bool parse_version(std::string_view version, int& major, int& minor)
{
    if (version.size() != 8 || version.substr(0, 5) != "HTTP/")
        return false;

    if (version[6] != '.')
        return false;

    const auto major_view = version.substr(5, 1);
    const auto minor_view = version.substr(7, 1);

    const auto parse_component = [](std::string_view component, int& out) {
        const auto* begin = component.data();
        const auto* end = component.data() + component.size();
        const auto result = std::from_chars(begin, end, out);
        return result.ec == std::errc{} && result.ptr == end;
    };

    return parse_component(major_view, major) &&
           parse_component(minor_view, minor);
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

char ascii_lower(char ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return static_cast<char>(ch - 'A' + 'a');
    return ch;
}

bool ascii_iequals(std::string_view left, std::string_view right)
{
    if (left.size() != right.size())
        return false;

    for (std::size_t i = 0; i < left.size(); ++i) {
        if (ascii_lower(left[i]) != ascii_lower(right[i]))
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

bool request::connection_token_present(std::string_view token) const noexcept
{
    const auto value = header_value("Connection");
    if (!value)
        return false;

    std::size_t begin = 0;
    while (begin <= value->size()) {
        auto end = value->find(',', begin);
        if (end == std::string_view::npos)
            end = value->size();

        const auto part = trim_optional_whitespace(value->substr(begin, end - begin));
        if (ascii_iequals(part, token))
            return true;

        if (end == value->size())
            break;
        begin = end + 1;
    }

    return false;
}

bool request::keep_alive_requested() const noexcept
{
    if (version_major == 1 && version_minor == 0)
        return connection_token_present("keep-alive");

    if (version_major == 1 && version_minor == 1)
        return !connection_token_present("close");

    return false;
}

std::string request::version_string() const
{
    return "HTTP/" + std::to_string(version_major) + "." + std::to_string(version_minor);
}

std::expected<request, request_parse_error> request::parse(std::string_view header_block)
{
    const auto eol = header_block.find("\r\n");
    if (eol == std::string_view::npos)
        return std::unexpected(request_parse_error::malformed_request_line);

    const auto line = header_block.substr(0, eol);

    const auto sp1 = line.find(' ');
    if (sp1 == std::string_view::npos)
        return std::unexpected(request_parse_error::malformed_request_line);

    const auto sp2 = line.find(' ', sp1 + 1);
    if (sp2 == std::string_view::npos)
        return std::unexpected(request_parse_error::malformed_request_line);
    if (line.find(' ', sp2 + 1) != std::string_view::npos)
        return std::unexpected(request_parse_error::malformed_request_line);

    const auto method = line.substr(0, sp1);
    const auto target = line.substr(sp1 + 1, sp2 - (sp1 + 1));
    const auto version = line.substr(sp2 + 1);

    if (!is_valid_method(method))
        return std::unexpected(request_parse_error::invalid_method);
    if (!is_valid_target(target))
        return std::unexpected(request_parse_error::invalid_target);
    if (!is_valid_version(version))
        return std::unexpected(request_parse_error::invalid_version);

    request r;
    if (!parse_version(version, r.version_major, r.version_minor))
        return std::unexpected(request_parse_error::invalid_version);
    r.method.assign(method);
    r.target.assign(target);

    std::size_t line_begin = eol + 2;
    while (line_begin <= header_block.size()) {
        const auto line_end = header_block.find("\r\n", line_begin);
        if (line_end == std::string_view::npos)
            return std::unexpected(request_parse_error::invalid_header);

        if (line_end == line_begin)
            break;

        header_field header;
        if (!parse_header_line(header_block.substr(line_begin, line_end - line_begin),
                               header)) {
            return std::unexpected(request_parse_error::invalid_header);
        }

        r.headers.push_back(std::move(header));
        line_begin = line_end + 2;
    }

    return r;
}

} // namespace http

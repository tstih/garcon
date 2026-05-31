// Implementation of HTTP response serialization.
//
// This file implements serialization logic for the http::response structure,
// producing a complete HTTP/1.1 response message suitable for transmission
// over a TCP connection. It also provides helpers for constructing common
// plain-text responses.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "http/response.h"

#include <sstream>

namespace http {

namespace {

bool is_token_char(unsigned char ch)
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
        return (ch >= 'A' && ch <= 'Z') ||
               (ch >= 'a' && ch <= 'z') ||
               (ch >= '0' && ch <= '9');
    }
}

bool is_reserved_header(std::string_view name, bool content_type_written)
{
    if (header_name_equals(name, "Connection"))
        return true;
    if (header_name_equals(name, "Content-Length"))
        return true;
    if (content_type_written && header_name_equals(name, "Content-Type"))
        return true;

    return false;
}

} // namespace

bool response::valid_header_name(std::string_view name) noexcept
{
    if (name.empty())
        return false;

    for (const char ch : name) {
        if (!is_token_char(static_cast<unsigned char>(ch)))
            return false;
    }

    return true;
}

bool response::valid_header_value(std::string_view value) noexcept
{
    return value.find_first_of("\r\n") == std::string_view::npos;
}

std::string response::serialize(connection_policy policy) const
{
    const auto wire_body_size = content_length.value_or(body.size());
    const bool content_type_written =
        !content_type.empty() && valid_header_value(content_type);

    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << ' ' << reason << "\r\n";
    oss << "Connection: "
        << (policy == connection_policy::keep_alive ? "keep-alive" : "close")
        << "\r\n";
    oss << "Content-Length: " << wire_body_size << "\r\n";
    if (content_type_written)
        oss << "Content-Type: " << content_type << "\r\n";
    for (const auto& header : headers) {
        if (is_reserved_header(header.name, content_type_written))
            continue;
        if (!valid_header_name(header.name) || !valid_header_value(header.value))
            continue;

        oss << header.name << ": " << header.value << "\r\n";
    }
    oss << "\r\n";
    oss.write(body.data(), static_cast<std::streamsize>(body.size()));
    return oss.str();
}

response response::text(int s, std::string_view r, std::string_view msg)
{
    response resp;
    resp.status = s;
    resp.reason = r;
    resp.content_type = "text/plain; charset=utf-8";
    resp.body = std::string(msg);
    return resp;
}

} // namespace http

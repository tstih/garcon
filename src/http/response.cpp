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

std::string response::serialize() const
{
    const auto wire_body_size = content_length.value_or(body.size());

    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << ' ' << reason << "\r\n";
    oss << "Connection: close\r\n";
    oss << "Content-Length: " << wire_body_size << "\r\n";
    if (!content_type.empty())
        oss << "Content-Type: " << content_type << "\r\n";
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

// HTTP response representation.
//
// This file defines the http::response structure, which represents an HTTP
// response message produced by the server. It contains the status code,
// reason phrase, optional content type, and response body, and provides
// serialization into a wire-format HTTP/1.1 response.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include <string>
#include <string_view>

namespace http {

struct response
{
    int status = 200;
    std::string_view reason = "OK";
    std::string content_type;
    std::string body;

    // Serializes the response into an HTTP/1.1 response message.
    std::string serialize() const;

    // Constructs a plain-text response with the given status and message.
    static response text(int status,
                         std::string_view reason,
                         std::string_view msg);
};

} // namespace http

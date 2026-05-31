// HTTP message framing utilities.
//
// This file provides helpers for delimiting HTTP messages on top of a byte
// stream. The current implementation reads and returns the HTTP header block
// (terminated by "\r\n\r\n") using an external http::buffer for incremental
// accumulation.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include "config.h"
#include "http/buffer.h"
#include "net/stream.h"

#include <cstddef>
#include <expected>
#include <string_view>

namespace http {

enum class header_read_error
{
    closed,
    timeout,
    too_large,
    io_error,
};

// Reads from the stream until the end of the HTTP header block is available
// in the buffer or until max_bytes is reached. On success, returns a view of
// the header block within the buffer (including the terminating "\r\n\r\n").
// The returned view remains valid as long as the buffer is not resized or
// destroyed.
[[nodiscard]] std::expected<std::string_view, header_read_error> read_header_block(
    net::stream& s,
    buffer& b,
    std::size_t max_bytes = garcon::config::max_request_header_bytes);

} // namespace http

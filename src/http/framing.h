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

#include "http/buffer.h"
#include "net/socket.h"

#include <cstddef>
#include <optional>
#include <string_view>

namespace http {

// Reads from the socket until the end of the HTTP header block is available
// in the buffer or until max_bytes is reached. On success, returns a view of
// the header block within the buffer (including the terminating "\r\n\r\n").
// The returned view remains valid as long as the buffer is not resized or
// destroyed.
std::optional<std::string_view> read_header_block(net::socket& s,
                                                  buffer& b,
                                                  std::size_t max_bytes = 64 * 1024);

} // namespace http

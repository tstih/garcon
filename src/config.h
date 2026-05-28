// Tunable server limits and timeouts.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace garcon::config {

// Maximum size of any file served to a client.
inline constexpr std::uintmax_t max_file_bytes = 8U * 1024U * 1024U;

// Maximum cumulative bytes read while parsing the HTTP request header block.
inline constexpr std::size_t max_request_header_bytes = 64 * 1024;

// Back-off pause after a transient accept() failure before retrying.
inline constexpr auto accept_retry_delay = std::chrono::milliseconds(50);

// Per-connection socket I/O timeout applied to both reads and writes.
inline constexpr auto socket_io_timeout = std::chrono::seconds(5);

} // namespace garcon::config

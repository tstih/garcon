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
// Garcon stays intentionally small and fully in-memory for static responses, so
// 8 MiB keeps abuse bounded while remaining comfortable for typical assets.
inline constexpr std::uintmax_t max_file_bytes = 8U * 1024U * 1024U;

// Maximum cumulative bytes read while parsing the HTTP request header block.
// RFC 9112 does not mandate a fixed limit; 64 KiB is a conservative ceiling
// that keeps header-flood memory growth bounded.
inline constexpr std::size_t max_request_header_bytes = 64 * 1024;

// Back-off pause after a transient accept() failure before retrying.
// A short pause avoids a busy retry loop while keeping local recovery fast.
inline constexpr auto accept_retry_delay = std::chrono::milliseconds(50);

// Per-connection socket I/O timeout applied to active request reads and writes.
// Five seconds is long enough for ordinary local development and short enough
// to fail closed on stalled peers.
inline constexpr auto socket_io_timeout = std::chrono::seconds(5);

// Idle timeout for waiting on the next request over an HTTP keep-alive
// connection. This is intentionally longer than the active I/O timeout because
// a quiet, otherwise healthy connection is less suspicious than a stalled one.
inline constexpr auto keep_alive_idle_timeout = std::chrono::seconds(15);

// Maximum number of accepted queued-or-active connections allowed from a
// single source IP at once. This is a coarse fairness guard for the current
// worker-pool runtime, not a full rate limiter. Keeping it modest preserves
// headroom for local development while making noisy single-client behavior
// fail closed before it consumes the whole worker pool.
inline constexpr std::size_t max_connections_per_ip = 8;

} // namespace garcon::config

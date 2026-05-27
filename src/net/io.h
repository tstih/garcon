// Shared transport I/O result types.
//
// This file defines small status/result types used by both plain TCP and TLS
// stream implementations. Keeping these types transport-neutral allows the
// HTTP layer to reason about timeouts, orderly closes, and fatal I/O failures
// without knowing whether the underlying connection is encrypted.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include <cstddef>

namespace net {

enum class io_status
{
    ok,
    closed,
    timeout,
    error,
};

struct read_result
{
    io_status status = io_status::error;
    std::size_t bytes_read = 0;

    explicit operator bool() const
    {
        return status == io_status::ok;
    }
};

} // namespace net

// HTTP receive buffer for incremental parsing.
//
// This file defines the http::buffer class, a simple growable byte buffer used
// to accumulate incoming data from a stream. It exposes a writable span for
// reads, a commit operation to advance the logical size, and a string_view
// projection for lightweight parsing without copying.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace http {

class buffer
{
public:
    // Returns a writable span with at least min_free bytes available.
    // The returned span points to the free space at the end of the buffer.
    [[nodiscard]] std::span<std::byte> write_span(std::size_t min_free = 2048);

    // Commits n bytes previously written into the span returned by write_span().
    void commit(std::size_t n);

    // Returns the currently committed buffer contents as a string view.
    // This view is valid until the buffer is resized or destroyed.
    [[nodiscard]] std::string_view as_string_view() const;

private:
    std::vector<std::byte> _data;
    std::size_t _size = 0;
};

} // namespace http

// Implementation of the HTTP receive buffer.
//
// This file implements the mechanics of the http::buffer class, including
// dynamic growth of the underlying storage, commit of newly received data,
// and projection of the committed contents as a string view for parsing.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "http/buffer.h"

#include <algorithm>

namespace http {

std::span<std::byte> buffer::write_span(std::size_t min_free)
{
    if (_data.size() < _size + min_free)
        _data.resize(_size + min_free);

    return std::span<std::byte>(_data.data() + _size,
                                _data.size() - _size);
}

void buffer::commit(std::size_t n)
{
    _size += n;
    if (_size > _data.size())
        _size = _data.size();
}

std::string_view buffer::as_string_view() const
{
    const auto* p = reinterpret_cast<const char*>(_data.data());
    return std::string_view(p, _size);
}

} // namespace http

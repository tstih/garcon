// Implementation of HTTP message framing utilities.
//
// This file implements low-level helpers for reading an HTTP header block
// from a byte stream by accumulating data until the "\r\n\r\n" delimiter is
// encountered or a configured maximum size is exceeded.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "http/framing.h"

#include <string_view>

namespace http {

static std::size_t find_header_end(std::string_view v)
{
    const auto p = v.find("\r\n\r\n");
    if (p == std::string_view::npos)
        return std::string_view::npos;
    return p + 4;
}

header_read_result read_header_block(net::stream& s,
                                     buffer& b,
                                     std::size_t max_bytes)
{
    for (;;) {
        const auto view = b.as_string_view();
        const auto end = find_header_end(view);
        if (end != std::string_view::npos)
            return header_read_result{
                .status = header_read_status::ok,
                .header = view.substr(0, end),
            };

        if (view.size() >= max_bytes)
            return header_read_result{
                .status = header_read_status::too_large,
            };

        auto out = b.write_span();
        const auto rc = s.recv_some(out);
        if (rc.status == net::io_status::closed) {
            return header_read_result{
                .status = header_read_status::closed,
            };
        }

        if (rc.status == net::io_status::timeout) {
            return header_read_result{
                .status = header_read_status::timeout,
            };
        }

        if (rc.status == net::io_status::error) {
            return header_read_result{
                .status = header_read_status::error,
            };
        }

        b.commit(rc.bytes_read);
    }
}

} // namespace http

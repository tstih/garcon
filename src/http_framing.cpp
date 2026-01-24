#include "http_framing.h"

#include <string_view>

namespace http {

static std::size_t find_header_end(std::string_view v)
{
    const auto p = v.find("\r\n\r\n");
    if (p == std::string_view::npos)
        return std::string_view::npos;
    return p + 4;
}

std::optional<std::string_view> read_header_block(net::socket& s,
                                                  buffer& b,
                                                  std::size_t max_bytes)
{
    for (;;) {
        const auto view = b.as_string_view();
        const auto end = find_header_end(view);
        if (end != std::string_view::npos)
            return view.substr(0, end);

        if (view.size() >= max_bytes)
            return std::nullopt;

        auto out = b.write_span();
        const auto rc = s.recv_some(out);
        if (rc <= 0)
            return std::nullopt;

        b.commit(static_cast<std::size_t>(rc));
    }
}

} /* namespace http */
